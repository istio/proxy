"""Macros used for representing crates or annotations for existing crates"""

load(":common_utils.bzl", "parse_alias_rule")

def _workspace_member(version, sha256 = None):
    """Define information for extra workspace members

    Args:
        version (str): The semver of the crate to download. Must be an exact version.
        sha256 (str, optional): The sha256 checksum of the `.crate` file.

    Returns:
        string: A json encoded string of all inputs
    """
    return json.encode(struct(
        version = version,
        sha256 = sha256,
    ))

def _spec(
        package = None,
        version = None,
        artifact = None,
        lib = None,
        default_features = True,
        features = [],
        git = None,
        branch = None,
        tag = None,
        rev = None):
    """A constructor for a crate dependency.

    See [specifying dependencies][sd] in the Cargo book for more details.

    [sd]: https://doc.rust-lang.org/cargo/reference/specifying-dependencies.html

    Args:
        package (str, optional): The explicit name of the package (used when attempting to alias a crate).
        version (str, optional): The exact version of the crate. Cannot be used with `git`.
        artifact (str, optional): Set to "bin" to pull in a binary crate as an artifact dependency. Requires a nightly Cargo.
        lib (bool, optional): If using `artifact = "bin"`, additionally setting `lib = True` declares a dependency on both the package's library and binary, as opposed to just the binary.
        default_features (bool, optional): Maps to the `default-features` flag.
        features (list, optional): A list of features to use for the crate
        git (str, optional): The Git url to use for the crate. Cannot be used with `version`.
        branch (str, optional): The git branch of the remote crate. Tied with the `git` param. Only one of branch, tag or rev may be specified. Specifying `rev` is recommended for fully-reproducible builds.
        tag (str, optional): The git tag of the remote crate. Tied with the `git` param. Only one of branch, tag or rev may be specified. Specifying `rev` is recommended for fully-reproducible builds.
        rev (str, optional): The git revision of the remote crate. Tied with the `git` param. Only one of branch, tag or rev may be specified.

    Returns:
        string: A json encoded string of all inputs
    """
    return json.encode({
        k: v
        for k, v in {
            "artifact": artifact,
            "branch": branch,
            "default_features": default_features,
            "features": features,
            "git": git,
            "lib": lib,
            "package": package,
            "rev": rev,
            "tag": tag,
            "version": version,
        }.items()
        # The `cargo_toml` crate parses unstable fields to a flattened
        # BTreeMap<String, toml::Value> and toml::Value does not support null,
        # so we must omit null values.
        if v != None
    })

def _assert_absolute(label):
    """Ensure a given label is an absolute label

    Args:
        label (Label): The label to check
    """
    label_str = str(label)
    if not label_str.startswith("@"):
        fail("The labels must be absolute. Please update '{}'".format(
            label_str,
        ))

# This should be kept in sync crate_universe/extension.bzl.
def _annotation(
        version = "*",
        additive_build_file = None,
        additive_build_file_content = None,
        alias_rule = None,
        build_script_compile_data = None,
        build_script_data = None,
        build_script_tools = None,
        build_script_data_glob = None,
        build_script_deps = None,
        build_script_env = None,
        build_script_proc_macro_deps = None,
        build_script_rundir = None,
        build_script_rustc_env = None,
        build_script_toolchains = None,
        build_script_use_default_shell_env = None,
        compile_data = None,
        compile_data_glob = None,
        crate_features = None,
        data = None,
        data_glob = None,
        deps = None,
        extra_aliased_targets = None,
        gen_binaries = None,
        disable_pipelining = False,
        gen_build_script = None,
        patch_args = None,
        patch_tool = None,
        patches = None,
        proc_macro_deps = None,
        rustc_env = None,
        rustc_env_files = None,
        rustc_flags = None,
        shallow_since = None,
        override_targets = None):
    """A collection of extra attributes and settings for a particular crate

    Args:
        version (str, optional): The version or semver-conditions to match with a crate. The wildcard `*`
            matches any version, including prerelease versions.
        additive_build_file_content (str, optional): Extra contents to write to the bottom of generated BUILD files.
        additive_build_file (str, optional): A file containing extra contents to write to the bottom of
            generated BUILD files.
        alias_rule (str, optional): Alias rule to use instead of `native.alias()`.  Overrides [render_config](#render_config)'s
            'default_alias_rule'.
        build_script_compile_data (list, optional): A list of labels to add to a crate's `cargo_build_script::compile_data` attribute.
        build_script_data (list, optional): A list of labels to add to a crate's `cargo_build_script::data` attribute.
        build_script_tools (list, optional): A list of labels to add to a crate's `cargo_build_script::tools` attribute.
        build_script_data_glob (list, optional): A list of glob patterns to add to a crate's `cargo_build_script::data`
            attribute.
        build_script_deps (list, optional): A list of labels to add to a crate's `cargo_build_script::deps` attribute.
        build_script_env (dict, optional): Additional environment variables to set on a crate's
            `cargo_build_script::env` attribute.
        build_script_proc_macro_deps (list, optional): A list of labels to add to a crate's
            `cargo_build_script::proc_macro_deps` attribute.
        build_script_rundir (str, optional): An override for the build script's rundir attribute.
        build_script_rustc_env (dict, optional): Additional environment variables to set on a crate's
            `cargo_build_script::env` attribute.
        build_script_toolchains (list, optional): A list of labels to set on a crates's `cargo_build_script::toolchains` attribute.
        build_script_use_default_shell_env (int, optional): Whether or not to include the default shell environment for the build
            script action.
        compile_data (list, optional): A list of labels to add to a crate's `rust_library::compile_data` attribute.
        compile_data_glob (list, optional): A list of glob patterns to add to a crate's `rust_library::compile_data`
            attribute.
        crate_features (optional): A list of strings to add to a crate's `rust_library::crate_features`
            attribute.
        data (list, optional): A list of labels to add to a crate's `rust_library::data` attribute.
        data_glob (list, optional): A list of glob patterns to add to a crate's `rust_library::data` attribute.
        deps (list, optional): A list of labels to add to a crate's `rust_library::deps` attribute.
        extra_aliased_targets (dict, optional): A list of targets to add to the generated aliases in the root
            crate_universe repository.
        gen_binaries (list or bool, optional): As a list, the subset of the crate's bins that should get `rust_binary`
            targets produced. Or `True` to generate all, `False` to generate none.
        disable_pipelining (bool, optional): If True, disables pipelining for library targets for this crate.
        gen_build_script (bool, optional): An authorative flag to determine whether or not to produce
            `cargo_build_script` targets for the current crate.
        patch_args (list, optional): The `patch_args` attribute of a Bazel repository rule. See
            [http_archive.patch_args](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patch_args)
        patch_tool (string, optional): The `patch_tool` attribute of a Bazel repository rule. See
            [http_archive.patch_tool](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patch_tool)
        patches (list, optional): The `patches` attribute of a Bazel repository rule. See
            [http_archive.patches](https://docs.bazel.build/versions/main/repo/http.html#http_archive-patches)
        proc_macro_deps (list, optional): A list of labels to add to a crate's `rust_library::proc_macro_deps`
            attribute.
        rustc_env (dict, optional): Additional variables to set on a crate's `rust_library::rustc_env` attribute.
        rustc_env_files (list, optional): A list of labels to set on a crate's `rust_library::rustc_env_files`
            attribute.
        rustc_flags (list, optional): A list of strings to set on a crate's `rust_library::rustc_flags` attribute.
        shallow_since (str, optional): An optional timestamp used for crates originating from a git repository
            instead of a crate registry. This flag optimizes fetching the source code.
        override_targets (dict, optional): A dictionary of alternate targets to use when something depends on this crate to allow
            the parent repo to provide its own version of this dependency. Keys can be `proc-marco`, `custom-build`, `lib`, `bin`.

    Returns:
        string: A json encoded string containing the specified version and separately all other inputs.
    """
    if additive_build_file:
        _assert_absolute(additive_build_file)
    if patches:
        for patch in patches:
            _assert_absolute(patch)

    return json.encode((
        version,
        struct(
            additive_build_file = _stringify_label(additive_build_file),
            additive_build_file_content = additive_build_file_content,
            alias_rule = parse_alias_rule(alias_rule),
            build_script_compile_data = _stringify_list(build_script_compile_data),
            build_script_data = _stringify_list(build_script_data),
            build_script_tools = _stringify_list(build_script_tools),
            build_script_data_glob = build_script_data_glob,
            build_script_deps = _stringify_list(build_script_deps),
            build_script_env = build_script_env,
            build_script_proc_macro_deps = _stringify_list(build_script_proc_macro_deps),
            build_script_rundir = build_script_rundir,
            build_script_rustc_env = build_script_rustc_env,
            build_script_toolchains = _stringify_list(build_script_toolchains),
            build_script_use_default_shell_env = build_script_use_default_shell_env,
            compile_data = _stringify_list(compile_data),
            compile_data_glob = compile_data_glob,
            crate_features = crate_features,
            data = _stringify_list(data),
            data_glob = data_glob,
            deps = _stringify_list(deps),
            extra_aliased_targets = extra_aliased_targets,
            gen_binaries = gen_binaries,
            disable_pipelining = disable_pipelining,
            gen_build_script = gen_build_script,
            patch_args = patch_args,
            patch_tool = patch_tool,
            patches = _stringify_list(patches),
            proc_macro_deps = _stringify_list(proc_macro_deps),
            rustc_env = rustc_env,
            rustc_env_files = _stringify_list(rustc_env_files),
            rustc_flags = rustc_flags,
            shallow_since = shallow_since,
            override_targets = override_targets,
        ),
    ))

def _stringify_label(value):
    if not value:
        return value
    return str(value)

# In bzlmod, attributes of type `attr.label_list` end up as `Label`s not `str`,
# and the `json` module doesn't know how to serialize `Label`s,
# so we proactively convert them to strings before serializing.
def _stringify_list(values):
    if not values:
        return values

    if type(values) == "list":
        return [str(x) for x in values]

    # if values is a struct with a "selects" attribute, assume it was created with
    # crate.select() and map values for all platforms
    if type(values) == "struct" and type(values.selects) != "NoneType":
        new_selects = {}

        for k, v in values.selects.items():
            new_selects[k] = [str(x) for x in v]

        return struct(common = [str(x) for x in values.common], selects = new_selects)

    fail("Cannot stringify unknown type for list '{}'".format(values))

def _select(common, selects):
    """A Starlark Select for `crate.annotation()`.

    Args:
        common: A value that applies to all configurations.
        selects (dict): A dict of `target_triple` to values.

    Returns:
        struct: A struct representing the Starlark Select.
    """
    return struct(
        common = common,
        selects = selects,
    )

crate = struct(
    spec = _spec,
    annotation = _annotation,
    workspace_member = _workspace_member,
    select = _select,
)
