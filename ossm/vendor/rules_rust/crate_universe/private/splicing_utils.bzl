"""Utilities directly related to the `splicing` step of `cargo-bazel`."""

load(":common_utils.bzl", "CARGO_BAZEL_DEBUG", "CARGO_BAZEL_REPIN", "REPIN", "cargo_environ", "execute")

def splicing_config(resolver_version = "2"):
    """Various settings used to configure Cargo manifest splicing behavior.

    [rv]: https://doc.rust-lang.org/cargo/reference/resolver.html#resolver-versions

    Args:
        resolver_version (str, optional): The [resolver version][rv] to use in generated Cargo
            manifests. This flag is **only** used when splicing a manifest from direct package
            definitions. See `crates_repository::packages`.

    Returns:
        str: A json encoded string of the parameters provided
    """
    return json.encode(struct(
        resolver_version = resolver_version,
    ))

def kebab_case_keys(data):
    """Ensure the key value of the data given are kebab-case

    Args:
        data (dict): A deserialized json blob

    Returns:
        dict: The same `data` but with kebab-case keys
    """
    return {
        key.lower().replace("_", "-"): val
        for (key, val) in data.items()
    }

def compile_splicing_manifest(splicing_config, manifests, cargo_config_path, packages):
    """Produce a manifest containing required components for splicing a new Cargo workspace

    [cargo_config]: https://doc.rust-lang.org/cargo/reference/config.html
    [cargo_toml]: https://doc.rust-lang.org/cargo/reference/manifest.html

    Args:
        splicing_config (dict): A deserialized `splicing_config`
        manifests (dict): A mapping of paths to Bazel labels which represent [Cargo manifests][cargo_toml].
        cargo_config_path (str): The absolute path to a [Cargo config][cargo_config].
        packages (dict): A set of crates (packages) specifications to depend on

    Returns:
        dict: A dictionary representation of a `cargo_bazel::splicing::SplicingManifest`
    """

    # Deserialize information about direct packges
    direct_packages_info = {
        # Ensure the data is using kebab-case as that's what `cargo_toml::DependencyDetail` expects.
        pkg: kebab_case_keys(dict(json.decode(data)))
        for (pkg, data) in packages.items()
    }

    # Auto-generated splicer manifest values
    splicing_manifest_content = {
        "cargo_config": cargo_config_path,
        "direct_packages": direct_packages_info,
        "manifests": manifests,
    }

    return splicing_config | splicing_manifest_content

def _no_at_label(label):
    """Strips leading '@'s for stringified labels in the main repository for backwards-comaptibility reasons."""
    s = str(label)
    if s.startswith("@@//"):
        return s[2:]
    if s.startswith("@//"):
        return s[1:]
    return s

def create_splicing_manifest(repository_ctx):
    """Produce a manifest containing required components for splicing a new Cargo workspace

    Args:
        repository_ctx (repository_ctx): The rule's context object.

    Returns:
        path: The path to a json encoded manifest
    """

    manifests = {str(repository_ctx.path(m)): _no_at_label(m) for m in repository_ctx.attr.manifests}

    if repository_ctx.attr.cargo_config:
        cargo_config = str(repository_ctx.path(repository_ctx.attr.cargo_config))
    else:
        cargo_config = None

    # Load user configurable splicing settings
    config = json.decode(repository_ctx.attr.splicing_config or splicing_config())

    splicing_manifest = repository_ctx.path("splicing_manifest.json")

    data = compile_splicing_manifest(
        splicing_config = config,
        manifests = manifests,
        cargo_config_path = cargo_config,
        packages = repository_ctx.attr.packages,
    )

    # Serialize information required for splicing
    repository_ctx.file(
        splicing_manifest,
        json.encode_indent(
            data,
            indent = " " * 4,
        ),
    )

    return splicing_manifest

def splice_workspace_manifest(repository_ctx, generator, cargo_lockfile, splicing_manifest, config_path, cargo, rustc):
    """Splice together a Cargo workspace from various other manifests and package definitions

    Args:
        repository_ctx (repository_ctx): The rule's context object.
        generator (path): The `cargo-bazel` binary.
        cargo_lockfile (path): The path to a "Cargo.lock" file.
        splicing_manifest (path): The path to a splicing manifest.
        config_path: The path to the config file (containing `cargo_bazel::config::Config`.)
        cargo (path): The path to a Cargo binary.
        rustc (path): The Path to a Rustc binary.

    Returns:
        path: The path to a Cargo metadata json file found in the spliced workspace root.
    """
    repository_ctx.report_progress("Splicing Cargo workspace.")

    splicing_output_dir = repository_ctx.path("splicing-output")

    # Generate a workspace root which contains all workspace members
    arguments = [
        generator,
        "splice",
        "--output-dir",
        splicing_output_dir,
        "--splicing-manifest",
        splicing_manifest,
        "--config",
        config_path,
        "--cargo",
        cargo,
        "--rustc",
        rustc,
        "--cargo-lockfile",
        cargo_lockfile,
        "--nonhermetic-root-bazel-workspace-dir",
        repository_ctx.workspace_root,
    ]

    # Optionally set the splicing workspace directory to somewhere within the repository directory
    # to improve the debugging experience.
    if CARGO_BAZEL_DEBUG in repository_ctx.os.environ:
        arguments.extend([
            "--workspace-dir",
            repository_ctx.path("splicing-workspace"),
        ])

    env = {
        "CARGO": str(cargo),
        "RUSTC": str(rustc),
        "RUST_BACKTRACE": "full",
    }

    # Ensure the short hand repin variable is set to the full name.
    if REPIN in repository_ctx.os.environ and CARGO_BAZEL_REPIN not in repository_ctx.os.environ:
        env["CARGO_BAZEL_REPIN"] = repository_ctx.os.environ[REPIN]

    # Add any Cargo environment variables to the `cargo-bazel` execution
    env |= cargo_environ(repository_ctx)

    execute(
        repository_ctx = repository_ctx,
        args = arguments,
        env = env,
    )

    # This file must have been produced by the execution above.
    spliced_lockfile = repository_ctx.path(splicing_output_dir.get_child("Cargo.lock"))
    if not spliced_lockfile.exists:
        fail("Lockfile file does not exist: " + str(spliced_lockfile))
    spliced_metadata = repository_ctx.path(splicing_output_dir.get_child("metadata.json"))
    if not spliced_metadata.exists:
        fail("Metadata file does not exist: " + str(spliced_metadata))

    return spliced_metadata
