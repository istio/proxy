"""Rules for vendoring Bazel targets into existing workspaces"""

load("//crate_universe/private:generate_utils.bzl", "compile_config", "render_config")
load("//crate_universe/private:splicing_utils.bzl", "kebab_case_keys", generate_splicing_config = "splicing_config")
load("//crate_universe/private:urls.bzl", "CARGO_BAZEL_LABEL")
load("//rust/platform:triple_mappings.bzl", "SUPPORTED_PLATFORM_TRIPLES")

_UNIX_WRAPPER = """\
#!/usr/bin/env bash

set -euo pipefail

export RUNTIME_PWD="$(pwd)"
if [[ -z "${{BAZEL_REAL:-}}" ]]; then
    BAZEL_REAL="$(which bazel || echo 'bazel')"
fi

_ENVIRON=()
_ENVIRON+=(BAZEL_REAL="${{BAZEL_REAL}}")
_ENVIRON+=(BUILD_WORKSPACE_DIRECTORY="${{BUILD_WORKSPACE_DIRECTORY}}")
_ENVIRON+=(PATH="${{PATH}}")

if [[ -n "${{CARGO_BAZEL_DEBUG:-}}" ]]; then
    _ENVIRON+=(CARGO_BAZEL_DEBUG="${{CARGO_BAZEL_DEBUG}}")
fi

# The path needs to be preserved to prevent bazel from starting with different
# startup options (requiring a restart of bazel).
# If you provide an empty path, bazel starts itself with
# --default_system_javabase set to the empty string, but if you provide a path,
# it may set it to a value (eg. "/usr/local/buildtools/java/jdk11").
exec env - \\
"${{_ENVIRON[@]}}" \\
{env} \\
    "{bin}" \\
    {args} \\
    --nonhermetic-root-bazel-workspace-dir="${{BUILD_WORKSPACE_DIRECTORY}}" \\
    "$@"
"""

_WINDOWS_WRAPPER = """\
@ECHO OFF
set RUNTIME_PWD=%CD%
{env}

{bin} {args} --nonhermetic-root-bazel-workspace-dir=%BUILD_WORKSPACE_DIRECTORY% %*
exit %ERRORLEVEL%
"""

CARGO_BAZEL_GENERATOR_PATH = "CARGO_BAZEL_GENERATOR_PATH"

def _default_render_config():
    return json.decode(render_config())

def _runfiles_path(file, is_windows):
    if is_windows:
        runtime_pwd_var = "%RUNTIME_PWD%"
    else:
        runtime_pwd_var = "${RUNTIME_PWD}"

    return "{}/{}".format(runtime_pwd_var, file.short_path)

def _is_windows(ctx):
    toolchain = ctx.toolchains[Label("@rules_rust//rust:toolchain_type")]
    return toolchain.target_os == "windows"

def _get_output_package(ctx):
    # Determine output directory
    if ctx.attr.vendor_path.startswith("/"):
        output = ctx.attr.vendor_path
    else:
        output = "{}/{}".format(
            ctx.label.package,
            ctx.attr.vendor_path,
        )
    return output.lstrip("/")

def _write_data_file(ctx, name, data):
    file = ctx.actions.declare_file("{}.{}".format(ctx.label.name, name))
    ctx.actions.write(
        output = file,
        content = data,
    )
    return file

def _prepare_manifest_path(target):
    """Generate manifest paths that are resolvable by `cargo_bazel::SplicingManifest::resolve`

    Args:
        target (Target): A `crate_vendor.manifest` target

    Returns:
        str: A string representing the path to a manifest.
    """
    files = target[DefaultInfo].files.to_list()
    if len(files) != 1:
        fail("The manifest {} hand an unexpected number of files: {}".format(
            target.label,
            files,
        ))

    manifest = files[0]

    if target.label.workspace_root.startswith("external"):
        # The short path of an external file is expected to start with `../`
        if not manifest.short_path.startswith("../"):
            fail("Unexpected shortpath for {}: {}".format(
                manifest,
                manifest.short_path,
            ))
        return manifest.short_path.replace("../", "${output_base}/external/", 1)

    return "${build_workspace_directory}/" + manifest.short_path

def _write_splicing_manifest(ctx):
    # Manifests are required to be single files
    manifests = {_prepare_manifest_path(m): str(m.label) for m in ctx.attr.manifests}

    manifest = _write_data_file(
        ctx = ctx,
        name = "cargo-bazel-splicing-manifest.json",
        data = generate_splicing_manifest(
            packages = ctx.attr.packages,
            splicing_config = ctx.attr.splicing_config,
            cargo_config = ctx.attr.cargo_config,
            manifests = manifests,
            manifest_to_path = _prepare_manifest_path,
        ),
    )

    is_windows = _is_windows(ctx)

    args = ["--splicing-manifest", _runfiles_path(manifest, is_windows)]
    runfiles = [manifest] + ctx.files.manifests + ([ctx.file.cargo_config] if ctx.attr.cargo_config else [])
    return args, runfiles

def generate_splicing_manifest(packages, splicing_config, cargo_config, manifests, manifest_to_path):
    # Deserialize information about direct packages
    direct_packages_info = {
        # Ensure the data is using kebab-case as that's what `cargo_toml::DependencyDetail` expects.
        pkg: kebab_case_keys(dict(json.decode(data)))
        for (pkg, data) in packages.items()
    }

    config = json.decode(splicing_config or generate_splicing_config())
    splicing_manifest_content = {
        "cargo_config": str(manifest_to_path(cargo_config)) if cargo_config else None,
        "direct_packages": direct_packages_info,
        "manifests": manifests,
    }

    return json.encode_indent(
        dict(dict(config).items() + splicing_manifest_content.items()),
        indent = " " * 4,
    )

def _write_config_file(ctx):
    workspace_name = ctx.workspace_name
    if workspace_name == "__main__" or ctx.workspace_name == "_main":
        workspace_name = ""

    config = _write_data_file(
        ctx = ctx,
        name = "cargo-bazel-config.json",
        data = generate_config_file(
            ctx,
            mode = ctx.attr.mode,
            annotations = ctx.attr.annotations,
            generate_binaries = ctx.attr.generate_binaries,
            generate_build_scripts = ctx.attr.generate_build_scripts,
            generate_target_compatible_with = ctx.attr.generate_target_compatible_with,
            supported_platform_triples = ctx.attr.supported_platform_triples,
            repository_name = ctx.attr.repository_name,
            output_pkg = _get_output_package(ctx),
            workspace_name = workspace_name,
            render_config = dict(json.decode(ctx.attr.render_config)) if ctx.attr.render_config else None,
        ),
    )

    is_windows = _is_windows(ctx)
    args = ["--config", _runfiles_path(config, is_windows)]
    runfiles = [config] + ctx.files.manifests
    return args, runfiles

def generate_config_file(
        ctx,
        mode,
        annotations,
        generate_binaries,
        generate_build_scripts,
        generate_target_compatible_with,
        supported_platform_triples,
        repository_name,
        output_pkg,
        workspace_name,
        render_config,
        repository_ctx = None):
    """Writes the rendering config to cargo-bazel-config.json.

    Args:
        ctx: The rule's context.
        mode (str): The vendoring mode.
        annotations: Any annotations provided.
        generate_binaries (bool): Whether to generate binaries for the crates.
        generate_build_scripts (bool): Whether to generate BUILD.bazel files.
        generate_target_compatible_with (bool): DEPRECATED: Moved to `render_config`.
        supported_platform_triples (str): The platform triples to support in
            the generated BUILD.bazel files.
        repository_name (str): The name of the repository to generate.
        output_pkg: The path to the package containing the build files.
        workspace_name (str): The name of the workspace.
        render_config: The render config to use.
        repository_ctx (repository_ctx, optional): A repository context object
            used for enabling certain functionality.

    Returns:
        file: The cargo-bazel-config.json written.
    """
    default_render_config = _default_render_config()
    if render_config == None:
        render_config = default_render_config

    if mode == "local":
        build_file_base_template = "//{}/{{name}}-{{version}}:BUILD.bazel".format(output_pkg)
        if workspace_name != "":
            build_file_base_template = "@{}//{}/{{name}}-{{version}}:BUILD.bazel".format(workspace_name, output_pkg)
        crate_label_template = "//{}/{{name}}-{{version}}:{{target}}".format(
            output_pkg,
        )
    else:
        build_file_base_template = "//{}:BUILD.{{name}}-{{version}}.bazel".format(output_pkg)
        if workspace_name != "":
            build_file_base_template = "@{}//{}:BUILD.{{name}}-{{version}}.bazel".format(workspace_name, output_pkg)
        crate_label_template = render_config["crate_label_template"]

    # If `workspace_name` is blank (such as when using modules), the `@{}//{}:{{file}}` template would generate
    # a reference like `Label(@//<stuff>)`. This causes issues if the module doing the `crates_vendor`ing is not the root module.
    # See: https://github.com/bazelbuild/rules_rust/issues/2661
    crates_module_template_value = "//{}:{{file}}".format(output_pkg)
    if workspace_name != "":
        crates_module_template_value = "@{}//{}:{{file}}".format(
            workspace_name,
            output_pkg,
        )

    updates = {
        "build_file_template": build_file_base_template,
        "crate_label_template": crate_label_template,
        "crates_module_template": crates_module_template_value,
        "vendor_mode": mode,
    }

    # "crate_label_template" is explicitly supported above in non-local modes
    excluded_from_key_check = ["crate_label_template"]

    for key in updates:
        if (render_config[key] != default_render_config[key]) and key not in excluded_from_key_check:
            fail("The `crates_vendor.render_config` attribute does not support the `{}` parameter. Please update {} to remove this value.".format(
                key,
                ctx.label,
            ))

    render_config.update(updates)

    # Allow users to override the regen command.
    if "regen_command" not in render_config or not render_config["regen_command"]:
        render_config.update({"regen_command": "bazel run {}".format(ctx.label)})

    config_data = compile_config(
        crate_annotations = annotations,
        generate_binaries = generate_binaries,
        generate_build_scripts = generate_build_scripts,
        generate_target_compatible_with = generate_target_compatible_with,
        cargo_config = None,
        render_config = render_config,
        supported_platform_triples = supported_platform_triples,
        repository_name = repository_name or ctx.label.name,
        repository_ctx = repository_ctx,
    )

    return json.encode_indent(
        config_data,
        indent = " " * 4,
    )

def _crates_vendor_impl(ctx):
    toolchain = ctx.toolchains[Label("@rules_rust//rust:toolchain_type")]
    is_windows = _is_windows(ctx)

    environ = {
        "CARGO": _runfiles_path(toolchain.cargo, is_windows),
        "RUSTC": _runfiles_path(toolchain.rustc, is_windows),
    }

    args = ["vendor"]

    cargo_bazel_runfiles = []

    # Allow action envs to override the use of the cargo-bazel target.
    if CARGO_BAZEL_GENERATOR_PATH in ctx.var:
        bin_path = ctx.var[CARGO_BAZEL_GENERATOR_PATH]
    elif ctx.executable.cargo_bazel:
        bin_path = _runfiles_path(ctx.executable.cargo_bazel, is_windows)
        cargo_bazel_runfiles.append(ctx.executable.cargo_bazel)
    else:
        fail("{} is missing either the `cargo_bazel` attribute or the '{}' action env".format(
            ctx.label,
            CARGO_BAZEL_GENERATOR_PATH,
        ))

    # Generate config file
    config_args, config_runfiles = _write_config_file(ctx)
    args.extend(config_args)
    cargo_bazel_runfiles.extend(config_runfiles)

    # Generate splicing manifest
    splicing_manifest_args, splicing_manifest_runfiles = _write_splicing_manifest(ctx)
    args.extend(splicing_manifest_args)
    cargo_bazel_runfiles.extend(splicing_manifest_runfiles)

    # Add an optional `Cargo.lock` file.
    if ctx.attr.cargo_lockfile:
        args.extend([
            "--cargo-lockfile",
            _runfiles_path(ctx.file.cargo_lockfile, is_windows),
        ])
        cargo_bazel_runfiles.extend([ctx.file.cargo_lockfile])

    # Optionally include buildifier
    if ctx.attr.buildifier:
        args.extend(["--buildifier", _runfiles_path(ctx.executable.buildifier, is_windows)])
        cargo_bazel_runfiles.append(ctx.executable.buildifier)

    # Optionally include an explicit `bazel` path
    if ctx.attr.bazel:
        args.extend(["--bazel", _runfiles_path(ctx.executable.bazel, is_windows)])
        cargo_bazel_runfiles.append(ctx.executable.bazel)

    # Determine platform specific settings
    if is_windows:
        extension = ".bat"
        template = _WINDOWS_WRAPPER
        env_template = "\nset {}={}"
    else:
        extension = ".sh"
        template = _UNIX_WRAPPER
        env_template = "{}={}"

    # Write the wrapper script
    runner = ctx.actions.declare_file(ctx.label.name + extension)
    ctx.actions.write(
        output = runner,
        content = template.format(
            env = " ".join([env_template.format(key, val) for key, val in environ.items()]),
            bin = bin_path,
            args = " ".join(args),
        ),
        is_executable = True,
    )

    return DefaultInfo(
        files = depset([runner]),
        runfiles = ctx.runfiles(
            files = cargo_bazel_runfiles,
            transitive_files = toolchain.all_files,
        ),
        executable = runner,
    )

CRATES_VENDOR_ATTRS = {
    "annotations": attr.string_list_dict(
        doc = "Extra settings to apply to crates. See [crate.annotation](#crateannotation).",
    ),
    "bazel": attr.label(
        doc = "The path to a bazel binary used to locate the output_base for the current workspace.",
        cfg = "exec",
        executable = True,
        allow_files = True,
    ),
    "buildifier": attr.label(
        doc = "The path to a [buildifier](https://github.com/bazelbuild/buildtools/blob/5.0.1/buildifier/README.md) binary used to format generated BUILD files.",
        cfg = "exec",
        executable = True,
        allow_files = True,
        default = Label("//crate_universe/private/vendor:buildifier"),
    ),
    "cargo_bazel": attr.label(
        doc = (
            "The cargo-bazel binary to use for vendoring. If this attribute is not set, then a " +
            "`{}` action env will be used.".format(CARGO_BAZEL_GENERATOR_PATH)
        ),
        cfg = "exec",
        executable = True,
        allow_files = True,
        default = CARGO_BAZEL_LABEL,
    ),
    "cargo_config": attr.label(
        doc = "A [Cargo configuration](https://doc.rust-lang.org/cargo/reference/config.html) file.",
        allow_single_file = True,
    ),
    "cargo_lockfile": attr.label(
        doc = "The path to an existing `Cargo.lock` file",
        allow_single_file = True,
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
    "manifests": attr.label_list(
        doc = "A list of Cargo manifests (`Cargo.toml` files).",
        allow_files = ["Cargo.toml"],
    ),
    "mode": attr.string(
        doc = (
            "Flags determining how crates should be vendored. `local` is where crate source and BUILD files are " +
            "written to the repository. `remote` is where only BUILD files are written and repository rules " +
            "used to fetch source code."
        ),
        values = [
            "local",
            "remote",
        ],
        default = "remote",
    ),
    "packages": attr.string_dict(
        doc = "A set of crates (packages) specifications to depend on. See [crate.spec](#crate.spec).",
    ),
    "render_config": attr.string(
        doc = (
            "The configuration flags to use for rendering. Use `//crate_universe:defs.bzl\\%render_config` to " +
            "generate the value for this field. If unset, the defaults defined there will be used."
        ),
    ),
    "repository_name": attr.string(
        doc = "The name of the repository to generate for `remote` vendor modes. If unset, the label name will be used",
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
    "vendor_path": attr.string(
        doc = "The path to a directory to write files into. Absolute paths will be treated as relative to the workspace root",
        default = "crates",
    ),
}

crates_vendor = rule(
    implementation = _crates_vendor_impl,
    doc = """\
A rule for defining Rust dependencies (crates) and writing targets for them to the current workspace.
This rule is useful for users whose workspaces are expected to be consumed in other workspaces as the
rendered `BUILD` files reduce the number of workspace dependencies, allowing for easier loads. This rule
handles all the same [workflows](#workflows) `crate_universe` rules do.

Example:

Given the following workspace structure:

```text
[workspace]/
    WORKSPACE
    BUILD
    Cargo.toml
    3rdparty/
        BUILD
    src/
        main.rs
```

The following is something that'd be found in `3rdparty/BUILD`:

```python
load("@rules_rust//crate_universe:defs.bzl", "crates_vendor", "crate")

crates_vendor(
    name = "crates_vendor",
    annotations = {
        "rand": [crate.annotation(
            default_features = False,
            features = ["small_rng"],
        )],
    },
    cargo_lockfile = "//:Cargo.Bazel.lock",
    manifests = ["//:Cargo.toml"],
    mode = "remote",
    vendor_path = "crates",
    tags = ["manual"],
)
```

The above creates a target that can be run to write `BUILD` files into the `3rdparty`
directory next to where the target is defined. To run it, simply call:

```shell
bazel run //3rdparty:crates_vendor
```

<a id="#crates_vendor_repinning_updating_dependencies"></a>

### Repinning / Updating Dependencies

Repinning dependencies is controlled by both the `CARGO_BAZEL_REPIN` environment variable or the `--repin`
flag to the `crates_vendor` binary. To update dependencies, simply add the flag ro your `bazel run` invocation.

```shell
bazel run //3rdparty:crates_vendor -- --repin
```

Under the hood, `--repin` will trigger a [cargo update](https://doc.rust-lang.org/cargo/commands/cargo-update.html)
call against the generated workspace. The following table describes how to control particular values passed to the
`cargo update` command.

| Value | Cargo command |
| --- | --- |
| Any of [`true`, `1`, `yes`, `on`, `workspace`] | `cargo update --workspace` |
| Any of [`full`, `eager`, `all`] | `cargo update` |
| `package_name` | `cargo upgrade --package package_name` |
| `package_name@1.2.3` | `cargo upgrade --package package_name --precise 1.2.3` |

""",
    attrs = CRATES_VENDOR_ATTRS,
    executable = True,
    toolchains = ["@rules_rust//rust:toolchain_type"],
)

def _crates_vendor_remote_repository_impl(repository_ctx):
    build_file = repository_ctx.path(repository_ctx.attr.build_file)
    defs_module = repository_ctx.path(repository_ctx.attr.defs_module)

    repository_ctx.file("BUILD.bazel", repository_ctx.read(build_file))
    repository_ctx.file("defs.bzl", repository_ctx.read(defs_module))
    repository_ctx.file("crates.bzl", "")
    repository_ctx.file("WORKSPACE.bazel", """workspace(name = "{}")""".format(
        repository_ctx.name,
    ))

crates_vendor_remote_repository = repository_rule(
    doc = "Creates a repository paired with `crates_vendor` targets using the `remote` vendor mode.",
    implementation = _crates_vendor_remote_repository_impl,
    attrs = {
        "build_file": attr.label(
            doc = "The BUILD file to use for the root package",
            mandatory = True,
        ),
        "defs_module": attr.label(
            doc = "The `defs.bzl` file to use in the repository",
            mandatory = True,
        ),
    },
)
