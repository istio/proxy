"""`crates_repository` rule implementation"""

load("//crate_universe/private:common_utils.bzl", "get_rust_tools")
load(
    "//crate_universe/private:generate_utils.bzl",
    "CRATES_REPOSITORY_ENVIRON",
    "determine_repin",
    "execute_generator",
    "generate_config",
    "get_generator",
    "get_lockfiles",
)
load(
    "//crate_universe/private:splicing_utils.bzl",
    "create_splicing_manifest",
    "splice_workspace_manifest",
)
load("//crate_universe/private:urls.bzl", "CARGO_BAZEL_SHA256S", "CARGO_BAZEL_URLS")
load("//rust:defs.bzl", "rust_common")
load("//rust/platform:triple.bzl", "get_host_triple")
load("//rust/platform:triple_mappings.bzl", "SUPPORTED_PLATFORM_TRIPLES")

def _crates_repository_impl(repository_ctx):
    # Determine the current host's platform triple
    host_triple = get_host_triple(repository_ctx)

    # Locate the generator to use
    generator, generator_sha256 = get_generator(repository_ctx, host_triple.str)

    # Generate a config file for all settings
    config_path = generate_config(repository_ctx)

    # Locate the lockfiles
    lockfiles = get_lockfiles(repository_ctx)

    # Locate Rust tools (cargo, rustc)
    tools = get_rust_tools(repository_ctx, host_triple)
    cargo_path = repository_ctx.path(tools.cargo)
    rustc_path = repository_ctx.path(tools.rustc)

    # Create a manifest of all dependency inputs
    splicing_manifest = create_splicing_manifest(repository_ctx)

    # Determine whether or not to repin depednencies
    repin = determine_repin(
        repository_ctx = repository_ctx,
        generator = generator,
        lockfile_path = lockfiles.bazel,
        config = config_path,
        splicing_manifest = splicing_manifest,
        cargo = cargo_path,
        rustc = rustc_path,
        repin_instructions = repository_ctx.attr.repin_instructions,
    )

    # If re-pinning is enabled, gather additional inputs for the generator
    kwargs = dict()
    if repin:
        # Generate a top level Cargo workspace and manifest for use in generation
        metadata_path = splice_workspace_manifest(
            repository_ctx = repository_ctx,
            generator = generator,
            cargo_lockfile = lockfiles.cargo,
            splicing_manifest = splicing_manifest,
            config_path = config_path,
            cargo = cargo_path,
            rustc = rustc_path,
        )

        kwargs.update({
            "metadata": metadata_path,
        })

    paths_to_track_file = repository_ctx.path("paths-to-track")
    warnings_output_file = repository_ctx.path("warnings-output-file")

    # Run the generator
    execute_generator(
        repository_ctx = repository_ctx,
        generator = generator,
        config = config_path,
        splicing_manifest = splicing_manifest,
        lockfile_path = lockfiles.bazel,
        cargo_lockfile_path = lockfiles.cargo,
        repository_dir = repository_ctx.path("."),
        cargo = cargo_path,
        rustc = rustc_path,
        paths_to_track_file = paths_to_track_file,
        warnings_output_file = warnings_output_file,
        # sysroot = tools.sysroot,
        **kwargs
    )

    paths_to_track = json.decode(repository_ctx.read(paths_to_track_file))
    for path in paths_to_track:
        # This read triggers watching the file at this path and invalidates the repository_rule which will get re-run.
        # Ideally we'd use repository_ctx.watch, but it doesn't support files outside of the workspace, and we need to support that.
        repository_ctx.read(path)

    warnings_output_file = json.decode(repository_ctx.read(warnings_output_file))
    for warning in warnings_output_file:
        # buildifier: disable=print
        print("WARN: {}".format(warning))

    # Determine the set of reproducible values
    attrs = {attr: getattr(repository_ctx.attr, attr) for attr in dir(repository_ctx.attr)}
    exclude = ["to_json", "to_proto"]
    for attr in exclude:
        attrs.pop(attr, None)

    # Note that this is only scoped to the current host platform. Users should
    # ensure they provide all the values necessary for the host environments
    # they support
    if generator_sha256:
        attrs.update({"generator_sha256s": generator_sha256})

    # Inform users that the repository rule can be made deterministic if they
    # add a label to a lockfile path specifically for Bazel.
    if not lockfiles.bazel:
        attrs.update({"lockfile": repository_ctx.attr.cargo_lockfile.relative("cargo-bazel-lock.json")})

    return attrs

crates_repository = repository_rule(
    doc = """\
A rule for defining and downloading Rust dependencies (crates). This rule
handles all the same [workflows](#workflows) `crate_universe` rules do.

Environment Variables:

| variable | usage |
| --- | --- |
| `CARGO_BAZEL_GENERATOR_SHA256` | The sha256 checksum of the file located at `CARGO_BAZEL_GENERATOR_URL` |
| `CARGO_BAZEL_GENERATOR_URL` | The URL of a cargo-bazel binary. This variable takes precedence over attributes and can use `file://` for local paths |
| `CARGO_BAZEL_ISOLATED` | An authorative flag as to whether or not the `CARGO_HOME` environment variable should be isolated from the host configuration |
| `CARGO_BAZEL_REPIN` | An indicator that the dependencies represented by the rule should be regenerated. `REPIN` may also be used. See [Repinning / Updating Dependencies](#repinning--updating-dependencies) for more details. |
| `CARGO_BAZEL_REPIN_ONLY` | A comma-delimited allowlist for rules to execute repinning. Can be useful if multiple instances of the repository rule are used in a Bazel workspace, but repinning should be limited to one of them. |

Example:

Given the following workspace structure:

```text
[workspace]/
    WORKSPACE.bazel
    BUILD.bazel
    Cargo.toml
    Cargo.Bazel.lock
    src/
        main.rs
```

The following is something that'd be found in the `WORKSPACE` file:

```python
load("@rules_rust//crate_universe:defs.bzl", "crates_repository", "crate")

crates_repository(
    name = "crate_index",
    annotations = {
        "rand": [crate.annotation(
            default_features = False,
            features = ["small_rng"],
        )],
    },
    cargo_lockfile = "//:Cargo.Bazel.lock",
    lockfile = "//:cargo-bazel-lock.json",
    manifests = ["//:Cargo.toml"],
    # Should match the version represented by the currently registered `rust_toolchain`.
    rust_version = "1.60.0",
)
```

The above will create an external repository which contains aliases and macros for accessing
Rust targets found in the dependency graph defined by the given manifests.

**NOTE**: The `cargo_lockfile` and `lockfile` must be manually created. The rule unfortunately does not yet create
it on its own. When initially setting up this rule, an empty file should be created and then
populated by repinning dependencies.

### Repinning / Updating Dependencies

Dependency syncing and updating is done in the repository rule which means it's done during the
analysis phase of builds. As mentioned in the environments variable table above, the `CARGO_BAZEL_REPIN`
(or `REPIN`) environment variables can be used to force the rule to update dependencies and potentially
render a new lockfile. Given an instance of this repository rule named `crate_index`, the easiest way to
repin dependencies is to run:

```shell
CARGO_BAZEL_REPIN=1 bazel sync --only=crate_index
```

This will result in all dependencies being updated for a project. The `CARGO_BAZEL_REPIN` environment variable
can also be used to customize how dependencies are updated. The following table shows translations from environment
variable values to the equivilant [cargo update](https://doc.rust-lang.org/cargo/commands/cargo-update.html) command
that is called behind the scenes to update dependencies.

| Value | Cargo command |
| --- | --- |
| Any of [`true`, `1`, `yes`, `on`, `workspace`] | `cargo update --workspace` |
| Any of [`full`, `eager`, `all`] | `cargo update` |
| `package_name` | `cargo upgrade --package package_name` |
| `package_name@1.2.3` | `cargo upgrade --package package_name@1.2.3` |
| `package_name@1.2.3=4.5.6` | `cargo upgrade --package package_name@1.2.3 --precise=4.5.6` |

If the `crates_repository` is used multiple times in the same Bazel workspace (e.g. for multiple independent
Rust workspaces), it may additionally be useful to use the `CARGO_BAZEL_REPIN_ONLY` environment variable, which
limits execution of the repinning to one or multiple instances of the `crates_repository` rule via a comma-delimited
allowlist:

```shell
CARGO_BAZEL_REPIN=1 CARGO_BAZEL_REPIN_ONLY=crate_index bazel sync --only=crate_index
```

""",
    implementation = _crates_repository_impl,
    attrs = {
        "annotations": attr.string_list_dict(
            doc = "Extra settings to apply to crates. See [crate.annotation](#crateannotation).",
        ),
        "cargo_config": attr.label(
            doc = "A [Cargo configuration](https://doc.rust-lang.org/cargo/reference/config.html) file",
        ),
        "cargo_lockfile": attr.label(
            doc = (
                "The path used to store the `crates_repository` specific " +
                "[Cargo.lock](https://doc.rust-lang.org/cargo/guide/cargo-toml-vs-cargo-lock.html) file. " +
                "In the case that your `crates_repository` corresponds directly with an existing " +
                "`Cargo.toml` file which has a paired `Cargo.lock` file, that `Cargo.lock` file " +
                "should be used here, which will keep the versions used by cargo and bazel in sync."
            ),
            mandatory = True,
        ),
        "compressed_windows_toolchain_names": attr.bool(
            doc = "Wether or not the toolchain names of windows toolchains are expected to be in a `compressed` format.",
            default = True,
        ),
        "generate_binaries": attr.bool(
            doc = (
                "Whether to generate `rust_binary` targets for all the binary crates in every package. " +
                "By default only the `rust_library` targets are generated."
            ),
            default = False,
        ),
        "generate_build_scripts": attr.bool(
            doc = (
                "Whether or not to generate " +
                "[cargo build scripts](https://doc.rust-lang.org/cargo/reference/build-scripts.html) by default."
            ),
            default = True,
        ),
        "generate_target_compatible_with": attr.bool(
            doc = "DEPRECATED: Moved to `render_config`.",
            default = True,
        ),
        "generator": attr.string(
            doc = (
                "The absolute label of a generator. Eg. `@cargo_bazel_bootstrap//:cargo-bazel`. " +
                "This is typically used when bootstrapping"
            ),
        ),
        "generator_sha256s": attr.string_dict(
            doc = "Dictionary of `host_triple` -> `sha256` for a `cargo-bazel` binary.",
            default = CARGO_BAZEL_SHA256S,
        ),
        "generator_urls": attr.string_dict(
            doc = (
                "URL template from which to download the `cargo-bazel` binary. `{host_triple}` and will be " +
                "filled in according to the host platform."
            ),
            default = CARGO_BAZEL_URLS,
        ),
        "isolated": attr.bool(
            doc = (
                "If true, `CARGO_HOME` will be overwritten to a directory within the generated repository in " +
                "order to prevent other uses of Cargo from impacting having any effect on the generated targets " +
                "produced by this rule. For users who either have multiple `crate_repository` definitions in a " +
                "WORKSPACE or rapidly re-pin dependencies, setting this to false may improve build times. This " +
                "variable is also controled by `CARGO_BAZEL_ISOLATED` environment variable."
            ),
            default = True,
        ),
        "lockfile": attr.label(
            doc = (
                "The path to a file to use for reproducible renderings. " +
                "If set, this file must exist within the workspace (but can be empty) before this rule will work."
            ),
        ),
        "manifests": attr.label_list(
            doc = "A list of Cargo manifests (`Cargo.toml` files).",
        ),
        "packages": attr.string_dict(
            doc = "A set of crates (packages) specifications to depend on. See [crate.spec](#crate.spec).",
        ),
        "quiet": attr.bool(
            doc = "If stdout and stderr should not be printed to the terminal.",
            default = True,
        ),
        "render_config": attr.string(
            doc = (
                "The configuration flags to use for rendering. Use `//crate_universe:defs.bzl\\%render_config` to " +
                "generate the value for this field. If unset, the defaults defined there will be used."
            ),
        ),
        "repin_instructions": attr.string(
            doc = "Instructions to re-pin the repository if required. Many people have wrapper scripts for keeping dependencies up to date, and would like to point users to that instead of the default.",
        ),
        "rust_toolchain_cargo_template": attr.string(
            doc = (
                "The template to use for finding the host `cargo` binary. `{version}` (eg. '1.53.0'), " +
                "`{triple}` (eg. 'x86_64-unknown-linux-gnu'), `{arch}` (eg. 'aarch64'), `{vendor}` (eg. 'unknown'), " +
                "`{system}` (eg. 'darwin'), `{cfg}` (eg. 'exec'), `{channel}` (eg. 'stable'), and `{tool}` (eg. " +
                "'rustc.exe') will be replaced in the string if present."
            ),
            default = "@rust_{system}_{arch}__{triple}__{channel}_tools//:bin/{tool}",
        ),
        "rust_toolchain_rustc_template": attr.string(
            doc = (
                "The template to use for finding the host `rustc` binary. `{version}` (eg. '1.53.0'), " +
                "`{triple}` (eg. 'x86_64-unknown-linux-gnu'), `{arch}` (eg. 'aarch64'), `{vendor}` (eg. 'unknown'), " +
                "`{system}` (eg. 'darwin'), `{cfg}` (eg. 'exec'), `{channel}` (eg. 'stable'), and `{tool}` (eg. " +
                "'cargo.exe') will be replaced in the string if present."
            ),
            default = "@rust_{system}_{arch}__{triple}__{channel}_tools//:bin/{tool}",
        ),
        "rust_version": attr.string(
            doc = "The version of Rust the currently registered toolchain is using. Eg. `1.56.0`, or `nightly/2021-09-08`",
            default = rust_common.default_version,
        ),
        "splicing_config": attr.string(
            doc = (
                "The configuration flags to use for splicing Cargo maniests. Use `//crate_universe:defs.bzl\\%rsplicing_config` to " +
                "generate the value for this field. If unset, the defaults defined there will be used."
            ),
        ),
        "supported_platform_triples": attr.string_list(
            doc = "A set of all platform triples to consider when generating dependencies.",
            default = SUPPORTED_PLATFORM_TRIPLES,
        ),
    },
    environ = CRATES_REPOSITORY_ENVIRON,
)
