"""Utilities directly related to the `generate` step of `cargo-bazel`."""

load(
    ":common_utils.bzl",
    "CARGO_BAZEL_DEBUG",
    "CARGO_BAZEL_ISOLATED",
    "CARGO_BAZEL_TIMEOUT",
    "REPIN_ALLOWLIST_ENV_VAR",
    "REPIN_ENV_VARS",
    "parse_alias_rule",
)

CARGO_BAZEL_GENERATOR_SHA256 = "CARGO_BAZEL_GENERATOR_SHA256"
CARGO_BAZEL_GENERATOR_URL = "CARGO_BAZEL_GENERATOR_URL"

GENERATOR_ENV_VARS = [
    CARGO_BAZEL_GENERATOR_URL,
    CARGO_BAZEL_GENERATOR_SHA256,
]

CRATES_REPOSITORY_ENVIRON = GENERATOR_ENV_VARS + REPIN_ENV_VARS + [
    REPIN_ALLOWLIST_ENV_VAR,
    CARGO_BAZEL_ISOLATED,
    CARGO_BAZEL_DEBUG,
    CARGO_BAZEL_TIMEOUT,
]

def get_generator(repository_ctx, host_triple):
    """Query network resources to locate a `cargo-bazel` binary

    Args:
        repository_ctx (repository_ctx): The rule's context object.
        host_triple (string): A string representing the host triple

    Returns:
        tuple(path, dict): The path to a `cargo-bazel` binary and the host sha256 pairing.
            The pairing (dict) may be `None` if there is no need to update the attribute
    """
    use_environ = False
    for var in GENERATOR_ENV_VARS:
        if var in repository_ctx.os.environ:
            use_environ = True

    output = repository_ctx.path("cargo-bazel.exe" if "win" in repository_ctx.os.name else "cargo-bazel")

    # The `generator` attribute is the next highest priority behind
    # environment variables. We check those first before deciding to
    # use an explicitly provided variable.
    if not use_environ and repository_ctx.attr.generator:
        generator = repository_ctx.path(Label(repository_ctx.attr.generator))

        # Resolve a few levels of symlinks to ensure we're accessing the direct binary
        for _ in range(1, 100):
            real_generator = generator.realpath
            if real_generator == generator:
                break
            generator = real_generator
        return generator, None

    # The environment variable will take precedence if set
    if use_environ:
        generator_sha256 = repository_ctx.os.environ.get(CARGO_BAZEL_GENERATOR_SHA256)
        generator_url = repository_ctx.os.environ.get(CARGO_BAZEL_GENERATOR_URL)
    else:
        generator_sha256 = repository_ctx.attr.generator_sha256s.get(host_triple)
        generator_url = repository_ctx.attr.generator_urls.get(host_triple)

    if not generator_url:
        fail((
            "No generator URL was found either in the `CARGO_BAZEL_GENERATOR_URL` " +
            "environment variable or for the `{}` triple in the `generator_urls` attribute"
        ).format(host_triple))

    # Download the file into place
    if generator_sha256:
        repository_ctx.download(
            output = output,
            url = generator_url,
            sha256 = generator_sha256,
            executable = True,
        )
        return output, None

    result = repository_ctx.download(
        output = output,
        url = generator_url,
        executable = True,
    )

    return output, {host_triple: result.sha256}

def render_config(
        build_file_template = "//:BUILD.{name}-{version}.bazel",
        crate_label_template = "@{repository}__{name}-{version}//:{target}",
        crate_alias_template = "//:{name}-{version}",
        crate_repository_template = "{repository}__{name}-{version}",
        crates_module_template = "//:{file}",
        default_alias_rule = "alias",
        default_package_name = None,
        generate_cargo_toml_env_vars = True,
        generate_target_compatible_with = True,
        platforms_template = "@rules_rust//rust/platform:{triple}",
        regen_command = None,
        vendor_mode = None,
        generate_rules_license_metadata = False):
    """Various settings used to configure rendered outputs

    The template parameters each support a select number of format keys. A description of each key
    can be found below where the supported keys for each template can be found in the parameter docs

    | key | definition |
    | --- | --- |
    | `name` | The name of the crate. Eg `tokio` |
    | `repository` | The rendered repository name for the crate. Directly relates to `crate_repository_template`. |
    | `triple` | A platform triple. Eg `x86_64-unknown-linux-gnu` |
    | `version` | The crate version. Eg `1.2.3` |
    | `target` | The library or binary target of the crate |
    | `file` | The basename of a file |

    Args:
        build_file_template (str, optional): The base template to use for BUILD file names. The available format keys
            are [`{name}`, {version}`].
        crate_label_template (str, optional): The base template to use for crate labels. The available format keys
            are [`{repository}`, `{name}`, `{version}`, `{target}`].
        crate_repository_template (str, optional): The base template to use for Crate label repository names. The
            available format keys are [`{repository}`, `{name}`, `{version}`].
        crate_alias_template (str, optional): The template to use when referring to generated aliases within the external
            repository. The available format keys are [`{repository}`, `{name}`, `{version}`].
        crates_module_template (str, optional): The pattern to use for the `defs.bzl` and `BUILD.bazel`
            file names used for the crates module. The available format keys are [`{file}`].
        default_alias_rule (str, option): Alias rule to use when generating aliases for all crates.  Acceptable values
            are 'alias', 'dbg'/'fastbuild'/'opt' (transitions each crate's `compilation_mode`)  or a string
            representing a rule in the form '<label to .bzl>:<rule>' that takes a single label parameter 'actual'.
            See '@crate_index//:alias_rules.bzl' for an example.
        default_package_name (str, optional): The default package name to use in the rendered macros. This affects the
            auto package detection of things like `all_crate_deps`.
        generate_cargo_toml_env_vars (bool, optional): Whether to generate cargo_toml_env_vars targets. This is expected
            to be true except when bootstrapping.
        generate_target_compatible_with (bool, optional):  Whether to generate `target_compatible_with` annotations on
            the generated BUILD files.  This catches a `target_triple`being targeted that isn't declared in
            `supported_platform_triples`.
        platforms_template (str, optional): The base template to use for platform names.
            See [platforms documentation](https://docs.bazel.build/versions/main/platforms.html). The available format
            keys are [`{triple}`].
        regen_command (str, optional): An optional command to demonstrate how generated files should be regenerated.
        vendor_mode (str, optional): An optional configuration for rendirng content to be rendered into repositories.
        generate_rules_license_metadata (bool, optional): Whether to generate rules license metadata

    Returns:
        string: A json encoded struct to match the Rust `config::RenderConfig` struct
    """
    return json.encode(struct(
        build_file_template = build_file_template,
        crate_alias_template = crate_alias_template,
        crate_label_template = crate_label_template,
        crate_repository_template = crate_repository_template,
        crates_module_template = crates_module_template,
        default_alias_rule = parse_alias_rule(default_alias_rule),
        default_package_name = default_package_name,
        generate_cargo_toml_env_vars = generate_cargo_toml_env_vars,
        generate_rules_license_metadata = generate_rules_license_metadata,
        generate_target_compatible_with = generate_target_compatible_with,
        platforms_template = platforms_template,
        regen_command = regen_command,
        vendor_mode = vendor_mode,
    ))

def _crate_id(name, version):
    """Creates a `cargo_bazel::config::CrateId`.

    Args:
        name (str): The name of the crate
        version (str): The crate's version

    Returns:
        str: A serialized representation of a CrateId
    """
    return "{} {}".format(name, version)

def collect_crate_annotations(annotations, repository_name):
    """Deserialize and sanitize crate annotations.

    Args:
        annotations (dict): A mapping of crate names to lists of serialized annotations
        repository_name (str): The name of the repository that owns the annotations

    Returns:
        dict: A mapping of `cargo_bazel::config::CrateId` to sets of annotations
    """
    annotations = {name: [json.decode(a) for a in annotation] for name, annotation in annotations.items()}
    crate_annotations = {}
    for name, annotation in annotations.items():
        for (version, data) in annotation:
            if name == "*" and version != "*":
                fail(
                    "Wildcard crate names must have wildcard crate versions. " +
                    "Please update the `annotations` attribute of the {} crates_repository".format(
                        repository_name,
                    ),
                )
            id = _crate_id(name, version)
            if id in crate_annotations:
                fail("Found duplicate entries for {}".format(id))

            crate_annotations.update({id: data})
    return crate_annotations

def _read_cargo_config(repository_ctx):
    if repository_ctx.attr.cargo_config:
        config = repository_ctx.path(repository_ctx.attr.cargo_config)
        return repository_ctx.read(config)
    return None

def _update_render_config(config, repository_name):
    """Add the repository name to the render config

    Args:
        config (dict): A `render_config` struct
        repository_name (str): The name of the repository that owns the config

    Returns:
        struct: An updated `render_config`.
    """

    # Add the repository name as it's very relevant to rendering.
    config.update({"repository_name": repository_name})

    return struct(**config)

def _get_render_config(repository_ctx):
    if repository_ctx.attr.render_config:
        config = dict(json.decode(repository_ctx.attr.render_config))
    else:
        config = dict(json.decode(render_config()))

    if not config.get("regen_command"):
        config["regen_command"] = "bazel sync --only={}".format(
            repository_ctx.name,
        )

    return config

def compile_config(
        crate_annotations,
        generate_binaries,
        generate_build_scripts,
        generate_target_compatible_with,
        cargo_config,
        render_config,
        supported_platform_triples,
        repository_name,
        repository_ctx = None):
    """Create a config file for generating crate targets

    [cargo_config]: https://doc.rust-lang.org/cargo/reference/config.html

    Args:
        crate_annotations (dict): Extra settings to apply to crates. See
            `crates_repository.annotations` or `crates_vendor.annotations`.
        generate_binaries (bool): Whether to generate `rust_binary` targets for all bins.
        generate_build_scripts (bool): Whether or not to globally disable build scripts.
        generate_target_compatible_with (bool): DEPRECATED: Moved to `render_config`.
        cargo_config (str): The optional contents of a [Cargo config][cargo_config].
        render_config (dict): The deserialized dict of the `render_config` function.
        supported_platform_triples (list): A list of platform triples
        repository_name (str): The name of the repository being generated
        repository_ctx (repository_ctx, optional): A repository context object used for enabling
            certain functionality.

    Returns:
        struct: A struct matching a `cargo_bazel::config::Config`.
    """
    annotations = collect_crate_annotations(crate_annotations, repository_name)

    # Load additive build files if any have been provided.
    unexpected = []
    for name, data in annotations.items():
        f = data.pop("additive_build_file", None)
        if f and not repository_ctx:
            unexpected.append(name)
            f = None
        content = [x for x in [
            data.pop("additive_build_file_content", None),
            repository_ctx.read(Label(f)) if f else None,
        ] if x]
        if content:
            data.update({"additive_build_file_content": "\n".join(content)})

    if unexpected:
        fail("The following annotations use `additive_build_file` which is not supported for {}: {}".format(repository_name, unexpected))

    # Deprecated: Apply `generate_target_compatible_with` to `render_config`.
    if not generate_target_compatible_with:
        # buildifier: disable=print
        print("DEPRECATED: 'generate_target_compatible_with' has been moved to 'render_config'")
        render_config.update({"generate_target_compatible_with": False})

    config = struct(
        generate_binaries = generate_binaries,
        generate_build_scripts = generate_build_scripts,
        annotations = annotations,
        cargo_config = cargo_config,
        rendering = _update_render_config(
            config = render_config,
            repository_name = repository_name,
        ),
        supported_platform_triples = supported_platform_triples,
    )

    return config

def generate_config(repository_ctx):
    """Generate a config file from various attributes passed to the rule.

    Args:
        repository_ctx (repository_ctx): The rule's context object.

    Returns:
        struct: A struct containing the path to a config and it's contents
    """

    config = compile_config(
        crate_annotations = repository_ctx.attr.annotations,
        generate_binaries = repository_ctx.attr.generate_binaries,
        generate_build_scripts = repository_ctx.attr.generate_build_scripts,
        generate_target_compatible_with = repository_ctx.attr.generate_target_compatible_with,
        cargo_config = _read_cargo_config(repository_ctx),
        render_config = _get_render_config(repository_ctx),
        supported_platform_triples = repository_ctx.attr.supported_platform_triples,
        repository_name = repository_ctx.name,
        repository_ctx = repository_ctx,
    )

    config_path = repository_ctx.path("cargo-bazel.json")
    repository_ctx.file(
        config_path,
        json.encode_indent(config, indent = " " * 4),
    )

    return config_path

def get_lockfiles(repository_ctx):
    """_summary_

    Args:
        repository_ctx (repository_ctx): The rule's context object.

    Returns:
        struct: _description_
    """
    return struct(
        cargo = repository_ctx.path(repository_ctx.attr.cargo_lockfile),
        bazel = repository_ctx.path(repository_ctx.attr.lockfile) if repository_ctx.attr.lockfile else None,
    )

def determine_repin(
        *,
        repository_ctx,
        cargo_bazel_fn,
        repository_name,
        lockfile_path,
        config,
        splicing_manifest,
        repin_instructions = None):
    """Use the `cargo-bazel` binary to determine whether or not dependencies need to be re-pinned

    Args:
        repository_ctx (repository_ctx): The rule's context object.
        cargo_bazel_fn (callable): A callback for invoking the `cargo-bazel` binary.
        repository_name (str): The name of the repository being generated.
        config (path): The path to a `cargo-bazel` config file. See `generate_config`.
        splicing_manifest (path): The path to a `cargo-bazel` splicing manifest. See `create_splicing_manifest`
        lockfile_path (path): The path to a "lock" file for reproducible outputs.
        repin_instructions (optional string): Instructions to re-pin dependencies in your repository. Will be shown when re-pinning is required.

    Returns:
        bool: True if dependencies need to be re-pinned
    """

    # If a repin environment variable is set, always repin
    for var in REPIN_ENV_VARS:
        if var in repository_ctx.os.environ and repository_ctx.os.environ[var].lower() not in ["false", "no", "0", "off"]:
            # If a repin allowlist is present only force repin if name is in list
            if REPIN_ALLOWLIST_ENV_VAR in repository_ctx.os.environ:
                indices_to_repin = repository_ctx.os.environ[REPIN_ALLOWLIST_ENV_VAR].split(",")
                if repository_name in indices_to_repin:
                    return True
            else:
                return True

    # If a deterministic lockfile was not added then always repin
    if not lockfile_path:
        return True

    # Run the binary to check if a repin is needed
    result = cargo_bazel_fn(
        args = [
            "query",
            "--lockfile",
            lockfile_path,
            "--config",
            config,
            "--splicing-manifest",
            splicing_manifest,
        ],
        allow_fail = True,
    )

    # If it was determined repinning should occur but there was no
    # flag indicating repinning was requested, an error is raised
    # since repinning should be an explicit action
    if result.return_code:
        if repin_instructions:
            msg = ("\n".join([
                result.stderr,
                "The current `lockfile` is out of date for '{}'.".format(repository_name),
                repin_instructions,
            ]))
        else:
            msg = ("\n".join([
                result.stderr,
                (
                    "The current `lockfile` is out of date for '{}'. Please re-run " +
                    "bazel using `CARGO_BAZEL_REPIN=true` if this is expected " +
                    "and the lockfile should be updated."
                ).format(repository_name),
            ]))
        fail(msg)

    return False

def execute_generator(
        *,
        cargo_bazel_fn,
        lockfile_path,
        cargo_lockfile_path,
        config,
        splicing_manifest,
        repository_dir,
        nonhermetic_root_bazel_workspace_dir,
        paths_to_track_file,
        warnings_output_file,
        skip_cargo_lockfile_overwrite,
        strip_internal_dependencies_from_cargo_lockfile,
        metadata = None,
        generator_label = None):
    """Execute the `cargo-bazel` binary to produce `BUILD` and `.bzl` files.

    Args:
        cargo_bazel_fn (callable): A callback for invoking the `cargo-bazel` binary.
        lockfile_path (path): The path to a "lock" file (file used for reproducible renderings).
        cargo_lockfile_path (path): The path to a "Cargo.lock" file within the root workspace.
        config (path): The path to a `cargo-bazel` config file.
        splicing_manifest (path): The path to a `cargo-bazel` splicing manifest. See `create_splicing_manifest`
        repository_dir (path): The output path for the Bazel module and BUILD files.
        nonhermetic_root_bazel_workspace_dir (path): The path to the current workspace root
        paths_to_track_file (path): Path to file where generator should write which files should trigger re-generating as a JSON list.
        warnings_output_file (path): Path to file where generator should write warnings to print.
        skip_cargo_lockfile_overwrite (bool): Whether to skip writing the cargo lockfile back after resolving.
            You may want to set this if your dependency versions are maintained externally through a non-trivial set-up.
            But you probably don't want to set this.
        strip_internal_dependencies_from_cargo_lockfile (bool): Whether to strip internal dependencies from the cargo lockfile.
            You may want to use this if you want to maintain a cargo lockfile for bazel only.
            Bazel only requires external dependencies to be present in the lockfile.
            By removing internal dependencies, the lockfile changes less frequently which reduces merge conflicts
            in other lockfiles where the cargo lockfile's sha is stored.
        generator_label (Label): The label of the `generator` parameter.
        metadata (path, optional): The path to a Cargo metadata json file. If this is set, it indicates to
            the generator that repinning is required. This file must be adjacent to a `Cargo.toml` and
            `Cargo.lock` file.

    Returns:
        struct: The results of `repository_ctx.execute`.
    """
    args = [
        "generate",
        "--cargo-lockfile",
        cargo_lockfile_path,
        "--config",
        config,
        "--splicing-manifest",
        splicing_manifest,
        "--repository-dir",
        repository_dir,
        "--nonhermetic-root-bazel-workspace-dir",
        nonhermetic_root_bazel_workspace_dir,
        "--paths-to-track",
        paths_to_track_file,
        "--warnings-output-path",
        warnings_output_file,
    ]

    if generator_label:
        args.extend([
            "--generator",
            generator_label,
        ])

    if lockfile_path:
        args.extend([
            "--lockfile",
            lockfile_path,
        ])

    if skip_cargo_lockfile_overwrite:
        args.append("--skip-cargo-lockfile-overwrite")

    if strip_internal_dependencies_from_cargo_lockfile:
        args.append("--strip-internal-dependencies-from-cargo-lockfile")

    # Some components are not required unless re-pinning is enabled
    if metadata:
        args.extend([
            "--repin",
            "--metadata",
            metadata,
        ])

    result = cargo_bazel_fn(
        args = args,
    )

    return result
