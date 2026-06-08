"""Utilities directly related to the `splicing` step of `cargo-bazel`."""

load(":common_utils.bzl", "CARGO_BAZEL_DEBUG", "CARGO_BAZEL_REPIN", "REPIN")

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

    # Deserialize information about direct packages
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
    """Strips leading '@'s for stringified labels in the main repository for backwards-compatibility reasons."""
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

def splice_workspace_manifest(
        *,
        repository_ctx,
        cargo_bazel_fn,
        cargo_lockfile,
        splicing_manifest,
        config_path,
        output_dir,
        repository_name,
        skip_cargo_lockfile_overwrite,
        nonhermetic_root_bazel_workspace_dir,
        debug_workspace_dir = None):
    """Splice together a Cargo workspace from various other manifests and package definitions

    Args:
        repository_ctx (repository_ctx or module_ctx): The repository's context object.
        cargo_bazel_fn (callable): A callback for invoking the `cargo-bazel` binary.
        cargo_lockfile (path): The path to a "Cargo.lock" file.
        splicing_manifest (path): The path to a splicing manifest.
        config_path (path): The path to the config file (containing `cargo_bazel::config::Config`.)
        output_dir (path): THe location in which to write splicing outputs.
        repository_name (str): Name of the repository being generated.
        skip_cargo_lockfile_overwrite (bool): Whether to skip writing the cargo lockfile back after resolving.
            You may want to set this if your dependency versions are maintained externally through a non-trivial set-up.
            But you probably don't want to set this.
        nonhermetic_root_bazel_workspace_dir (path): The path to the current workspace root
        debug_workspace_dir (path): The location in which to save splicing outputs for future review.

    Returns:
        path: The path to a Cargo metadata json file found in the spliced workspace root.
    """

    # Generate a workspace root which contains all workspace members
    arguments = [
        "splice",
        "--output-dir",
        output_dir,
        "--splicing-manifest",
        splicing_manifest,
        "--config",
        config_path,
        "--repository-name",
        repository_name,
        "--nonhermetic-root-bazel-workspace-dir",
        nonhermetic_root_bazel_workspace_dir,
    ]

    if cargo_lockfile:
        arguments.extend([
            "--cargo-lockfile",
            cargo_lockfile,
        ])

    if skip_cargo_lockfile_overwrite:
        arguments.append("--skip-cargo-lockfile-overwrite")

    # Optionally set the splicing workspace directory to somewhere within the repository directory
    # to improve the debugging experience.
    if CARGO_BAZEL_DEBUG in repository_ctx.os.environ:
        if debug_workspace_dir == None:
            debug_workspace_dir = repository_ctx.path("splicing-workspace")
        arguments.extend([
            "--workspace-dir",
            debug_workspace_dir,
        ])

    env = {}

    # Ensure the short hand repin variable is set to the full name.
    if REPIN in repository_ctx.os.environ and CARGO_BAZEL_REPIN not in repository_ctx.os.environ:
        env["CARGO_BAZEL_REPIN"] = repository_ctx.os.environ[REPIN]

    cargo_bazel_fn(
        args = arguments,
        env = env,
    )

    # This file must have been produced by the execution above.
    spliced_lockfile = repository_ctx.path(output_dir.get_child("Cargo.lock"))
    if not spliced_lockfile.exists:
        fail("Lockfile file does not exist: " + str(spliced_lockfile))
    spliced_metadata = repository_ctx.path(output_dir.get_child("metadata.json"))
    if not spliced_metadata.exists:
        fail("Metadata file does not exist: " + str(spliced_metadata))

    extra_paths_to_track_path = repository_ctx.path(output_dir.get_child("extra_paths_to_track"))
    if extra_paths_to_track_path.exists:
        extra_paths_to_track = repository_ctx.read(extra_paths_to_track_path).split("\n")
    else:
        extra_paths_to_track = []

    return struct(
        metadata = spliced_metadata,
        cargo_lock = spliced_lockfile,
        extra_paths_to_track = extra_paths_to_track,
    )
