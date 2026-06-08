"""# Rust Repositories

Repository rules for defining Rust dependencies and toolchains.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//rust/platform:triple.bzl", "get_host_triple", "triple")
load("//rust/platform:triple_mappings.bzl", "triple_to_constraint_set")
load("//rust/private:common.bzl", "rust_common")
load("//rust/private:compat.bzl", "abs")
load(
    "//rust/private:repository_utils.bzl",
    "BUILD_for_rust_analyzer_proc_macro_srv",
    "BUILD_for_rust_analyzer_toolchain",
    "BUILD_for_rust_toolchain",
    "BUILD_for_rustfmt_toolchain",
    "BUILD_for_toolchain",
    "DEFAULT_EXTRA_TARGET_TRIPLES",
    "DEFAULT_NIGHTLY_VERSION",
    "DEFAULT_STATIC_RUST_URL_TEMPLATES",
    "TINYJSON_KWARGS",
    "check_version_valid",
    "includes_rust_analyzer_proc_macro_srv",
    "load_cargo",
    "load_clippy",
    "load_llvm_tools",
    "load_rust_analyzer",
    "load_rust_compiler",
    "load_rust_src",
    "load_rust_stdlib",
    "load_rustc_dev_nightly",
    "load_rustfmt",
    "select_rust_version",
    "toolchain_repository_hub",
    _load_arbitrary_tool = "load_arbitrary_tool",
)
load("//rust/private:semver.bzl", "semver")

# Re-export `load_arbitrary_tool` as it's historically been used in external repositories.
load_arbitrary_tool = _load_arbitrary_tool

# Note: Code in `.github/workflows/crate_universe.yaml` looks for this line, if you remove it or change its format, you will also need to update that code.
DEFAULT_TOOLCHAIN_TRIPLES = {
    "aarch64-apple-darwin": "rust_macos_aarch64",
    "aarch64-pc-windows-msvc": "rust_windows_aarch64",
    "aarch64-unknown-linux-gnu": "rust_linux_aarch64",
    "powerpc64le-unknown-linux-gnu": "rust_linux_powerpc64le",
    "s390x-unknown-linux-gnu": "rust_linux_s390x",
    "x86_64-apple-darwin": "rust_macos_x86_64",
    "x86_64-pc-windows-msvc": "rust_windows_x86_64",
    "x86_64-unknown-freebsd": "rust_freebsd_x86_64",
    "x86_64-unknown-linux-gnu": "rust_linux_x86_64",
}

_COMPACT_WINDOWS_NAMES = True

def rules_rust_dependencies():
    """Dependencies used in the implementation of `rules_rust`."""

    maybe(
        http_archive,
        name = "platforms",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/platforms/releases/download/0.0.11/platforms-0.0.11.tar.gz",
            "https://github.com/bazelbuild/platforms/releases/download/0.0.11/platforms-0.0.11.tar.gz",
        ],
        sha256 = "29742e87275809b5e598dc2f04d86960cc7a55b3067d97221c9abbc9926bff0f",
    )

    # Avoid the following issue https://github.com/bazelbuild/rules_cc/issues/274
    rules_cc_kwargs = {}
    if native.bazel_version.startswith(("6", "7")):
        rules_cc_kwargs.update({
            "patch_args": ["-p1"],
            "patches": [Label("//rust/private/3rdparty:rules_cc.patch")],
        })
    maybe(
        http_archive,
        name = "rules_cc",
        urls = ["https://github.com/bazelbuild/rules_cc/releases/download/0.0.17/rules_cc-0.0.17.tar.gz"],
        sha256 = "abc605dd850f813bb37004b77db20106a19311a96b2da1c92b789da529d28fe1",
        strip_prefix = "rules_cc-0.0.17",
        **rules_cc_kwargs
    )
    maybe(
        http_archive,
        name = "rules_license",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_license/releases/download/1.0.0/rules_license-1.0.0.tar.gz",
            "https://github.com/bazelbuild/rules_license/releases/download/1.0.0/rules_license-1.0.0.tar.gz",
        ],
        sha256 = "26d4021f6898e23b82ef953078389dd49ac2b5618ac564ade4ef87cced147b38",
    )
    maybe(
        http_archive,
        name = "rules_shell",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_shell/releases/download/v0.3.0/rules_shell-v0.3.0.tar.gz",
            "https://github.com/bazelbuild/rules_shell/releases/download/v0.3.0/rules_shell-v0.3.0.tar.gz",
        ],
        sha256 = "d8cd4a3a91fc1dc68d4c7d6b655f09def109f7186437e3f50a9b60ab436a0c53",
        strip_prefix = "rules_shell-0.3.0",
    )
    maybe(
        http_archive,
        name = "bazel_features",
        sha256 = "af3d4fb1cf4f25942cb4a933b1ad93a0ea9fe9ee70c2af7f369fb72a67c266e5",
        strip_prefix = "bazel_features-1.21.0",
        url = "https://github.com/bazel-contrib/bazel_features/releases/download/v1.21.0/bazel_features-v1.21.0.tar.gz",
    )

    maybe(
        http_archive,
        name = "bazel_skylib",
        sha256 = "bc283cdfcd526a52c3201279cda4bc298652efa898b10b4db0837dc51652756f",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.7.1/bazel-skylib-1.7.1.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.7.1/bazel-skylib-1.7.1.tar.gz",
        ],
    )

    # Make the iOS simulator constraint available, which is referenced in abi_to_constraints()
    # rules_rust does not require this dependency; it is just imported as a convenience for users.
    maybe(
        http_archive,
        name = "build_bazel_apple_support",
        sha256 = "b53f6491e742549f13866628ddffcc75d1f3b2d6987dc4f14a16b242113c890b",
        url = "https://github.com/bazelbuild/apple_support/releases/download/1.17.1/apple_support.1.17.1.tar.gz",
    )

    # process_wrapper needs a low-dependency way to process json.
    maybe(
        http_archive,
        **TINYJSON_KWARGS
    )

_RUST_TOOLCHAIN_VERSIONS = [
    rust_common.default_version,
    DEFAULT_NIGHTLY_VERSION,
]

# buildifier: disable=unnamed-macro
def rust_register_toolchains(
        *,
        dev_components = False,
        edition = None,
        allocator_library = None,
        global_allocator_library = None,
        register_toolchains = True,
        rustfmt_version = None,
        rust_analyzer_version = None,
        sha256s = None,
        extra_target_triples = DEFAULT_EXTRA_TARGET_TRIPLES,
        extra_rustc_flags = None,
        extra_exec_rustc_flags = None,
        strip_level = None,
        urls = DEFAULT_STATIC_RUST_URL_TEMPLATES,
        versions = _RUST_TOOLCHAIN_VERSIONS,
        aliases = {},
        hub_name = None,
        compact_windows_names = _COMPACT_WINDOWS_NAMES,
        toolchain_triples = DEFAULT_TOOLCHAIN_TRIPLES,
        rustfmt_toolchain_triples = DEFAULT_TOOLCHAIN_TRIPLES,
        target_settings = [],
        extra_toolchain_infos = None):
    """Emits a default set of toolchains for Linux, MacOS, and Freebsd

    Skip this macro and call the `rust_repository_set` macros directly if you need a compiler for \
    other hosts or for additional target triples.

    The `sha256s` attribute represents a dict associating tool subdirectories to sha256 hashes. As an example:
    ```python
    {
        "rust-1.46.0-x86_64-unknown-linux-gnu": "e3b98bc3440fe92817881933f9564389eccb396f5f431f33d48b979fa2fbdcf5",
        "rustfmt-1.4.12-x86_64-unknown-linux-gnu": "1894e76913303d66bf40885a601462844eec15fca9e76a6d13c390d7000d64b0",
        "rust-std-1.46.0-x86_64-unknown-linux-gnu": "ac04aef80423f612c0079829b504902de27a6997214eb58ab0765d02f7ec1dbc",
    }
    ```
    This would match for `exec_triple = "x86_64-unknown-linux-gnu"`.  If not specified, rules_rust pulls from a non-exhaustive \
    list of known checksums..

    See `load_arbitrary_tool` in `@rules_rust//rust:repositories.bzl` for more details.

    Args:
        dev_components (bool, optional): Whether to download the rustc-dev components (defaults to False). Requires version to be "nightly".
        edition (str, optional): The rust edition to be used by default (2015, 2018, or 2021). If absent, every target is required to specify its `edition` attribute.
        allocator_library (str, optional): Target that provides allocator functions when rust_library targets are embedded in a cc_binary.
        global_allocator_library (str, optional): Target that provides allocator functions when global allocator is used with cc_common.link.
        register_toolchains (bool): If true, repositories will be generated to produce and register `rust_toolchain` targets.
        rustfmt_version (str, optional): The version of rustfmt. If none is supplied and only a single version in `versions` is given, then this defaults to that version, otherwise will default to the default nightly version.
        rust_analyzer_version (str, optional): The version of Rustc to pair with rust-analyzer.
        sha256s (str, optional): A dict associating tool subdirectories to sha256 hashes.
        extra_target_triples (list, optional): Additional rust-style targets that rust toolchains should support.
        extra_rustc_flags (dict, list, optional): Dictionary of target triples to list of extra flags to pass to rustc in non-exec configuration.
        extra_exec_rustc_flags (list, optional): Extra flags to pass to rustc in exec configuration.
        strip_level (dict, dict, optional): Dictionary of target triples to strip config.
        urls (list, optional): A list of mirror urls containing the tools from the Rust-lang static file server. These must contain the '{}' used to substitute the tool being fetched (using .format).
        versions (list, optional): A list of toolchain versions to download. This parameter only accepts one versions
            per channel. E.g. `["1.65.0", "nightly/2022-11-02", "beta/2020-12-30"]`.
        aliases (dict, optional): A mapping of "full" repository name to another name to use instead.
        hub_name (str, optional): The name of the bzlmod hub repository for toolchains.
        compact_windows_names (bool): Whether or not to produce compact repository names for windows
            toolchains. This is to avoid MAX_PATH issues.
        toolchain_triples (dict[str, str], optional): Mapping of rust target triple -> repository name to create.
        rustfmt_toolchain_triples (dict[str, str], optional): Like toolchain_triples, but for rustfmt toolchains.
        target_settings (list of labels as strings, optional): A list of `config_settings` that must be satisfied by the target configuration in order for this toolchain to be selected during toolchain resolution.
        extra_toolchain_infos: (dict[str, dict], optional): Mapping of information about extra toolchains which were created outside of this call, which should be added to the hub repo.
    """
    if not rustfmt_version:
        if len(versions) == 1:
            rustfmt_version = versions[0]
        else:
            rustfmt_version = DEFAULT_NIGHTLY_VERSION

    if dev_components:
        has_nightly = False
        for ver in versions:
            if ver.startswith("nightly"):
                has_nightly = True
                break
        if not has_nightly:
            fail("rustc-dev components were requested but no \"nightly\" is being registered. Please update `versions` to include a nightly version.")

    if not rust_analyzer_version:
        rust_analyzer_version = select_rust_version(versions)

    # Convert to an unfrozen dict to remove mappings as they are used. This will allow us to determine if there
    # are any unused aliases requested.
    aliases = dict(aliases)

    rust_analyzer_repo_name = "rust_analyzer_{}".format(rust_analyzer_version.replace("/", "-"))

    toolchain_names = []
    toolchain_labels = {}
    toolchain_target_settings = {}
    toolchain_types = {}
    exec_compatible_with_by_toolchain = {}
    target_compatible_with_by_toolchain = {}

    maybe(
        rust_analyzer_toolchain_repository,
        name = rust_analyzer_repo_name,
        version = rust_analyzer_version,
        urls = urls,
        sha256s = sha256s,
    )

    toolchain_names.append(rust_analyzer_repo_name)
    toolchain_labels[rust_analyzer_repo_name] = "@{}_tools//:rust_analyzer_toolchain".format(
        rust_analyzer_repo_name,
    )
    exec_compatible_with_by_toolchain[rust_analyzer_repo_name] = []
    target_compatible_with_by_toolchain[rust_analyzer_repo_name] = []
    toolchain_types[rust_analyzer_repo_name] = "@rules_rust//rust/rust_analyzer:toolchain_type"

    if register_toolchains:
        native.register_toolchains("@{}//:toolchain".format(
            rust_analyzer_repo_name,
        ))

    for exec_triple, name in toolchain_triples.items():
        maybe(
            rust_repository_set,
            name = name,
            dev_components = dev_components,
            edition = edition,
            exec_triple = exec_triple,
            extra_target_triples = extra_target_triples,
            allocator_library = allocator_library,
            global_allocator_library = global_allocator_library,
            register_toolchain = register_toolchains,
            rustfmt_version = rustfmt_version,
            extra_rustc_flags = extra_rustc_flags,
            extra_exec_rustc_flags = extra_exec_rustc_flags,
            strip_level = strip_level,
            sha256s = sha256s,
            urls = urls,
            versions = versions,
            aliases = dict(aliases),
        )

        for toolchain in _get_toolchain_repositories(
            name = name,
            exec_triple = exec_triple,
            extra_target_triples = extra_target_triples,
            versions = versions,
            fallback_target_compatible_with = None,
            aliases = aliases,
            compact_windows_names = compact_windows_names,
        ):
            toolchain_names.append(toolchain.name)
            toolchain_labels[toolchain.name] = "@{}//:{}".format(toolchain.name + "_tools", "rust_toolchain")
            exec_compatible_with_by_toolchain[toolchain.name] = triple_to_constraint_set(exec_triple)
            target_compatible_with_by_toolchain[toolchain.name] = toolchain.target_constraints
            toolchain_types[toolchain.name] = "@rules_rust//rust:toolchain"
            toolchain_target_settings[toolchain.name] = ["@rules_rust//rust/toolchain/channel:{}".format(toolchain.channel.name)] + target_settings

    for exec_triple, name in rustfmt_toolchain_triples.items():
        rustfmt_repo_name = "rustfmt_{}__{}".format(rustfmt_version.replace("/", "-"), exec_triple)
        if rustfmt_repo_name in aliases:
            rustfmt_repo_name = aliases.pop(rustfmt_repo_name)

        maybe(
            rustfmt_toolchain_repository,
            name = rustfmt_repo_name,
            version = rustfmt_version,
            urls = urls,
            sha256s = sha256s,
            exec_triple = exec_triple,
        )

        if register_toolchains:
            native.register_toolchains("@{}//:toolchain".format(
                rustfmt_repo_name,
            ))

        toolchain_names.append(rustfmt_repo_name)
        toolchain_labels[rustfmt_repo_name] = "@{}_tools//:rustfmt_toolchain".format(rustfmt_repo_name)
        exec_compatible_with_by_toolchain[rustfmt_repo_name] = triple_to_constraint_set(exec_triple)
        target_compatible_with_by_toolchain[rustfmt_repo_name] = []
        toolchain_types[rustfmt_repo_name] = "@rules_rust//rust/rustfmt:toolchain_type"

    if aliases:
        fail("No repositories were created matching the requested names to alias:\n{}".format("\n".join(sorted(aliases))))

    if hub_name:
        if extra_toolchain_infos:
            for name, info in extra_toolchain_infos.items():
                toolchain_names.append(name)
                toolchain_labels[name] = info["tools_toolchain_label"]
                exec_compatible_with_by_toolchain[name] = info["exec_compatible_with"]
                target_compatible_with_by_toolchain[name] = info["target_compatible_with"]
                toolchain_target_settings[name] = info["target_settings"]
                toolchain_types[name] = info["toolchain_type"]

        toolchain_repository_hub(
            name = hub_name,
            toolchain_names = toolchain_names,
            toolchain_labels = toolchain_labels,
            toolchain_types = toolchain_types,
            target_settings = toolchain_target_settings,
            exec_compatible_with = exec_compatible_with_by_toolchain,
            target_compatible_with = target_compatible_with_by_toolchain,
        )

# buildifier: disable=unnamed-macro
def rust_repositories(**kwargs):
    """**Deprecated**: Use [rules_rust_dependencies](#rules_rust_dependencies) \
    and [rust_register_toolchains](#rust_register_toolchains) directly.

    Args:
        **kwargs (dict): Keyword arguments for the `rust_register_toolchains` macro.
    """
    rules_rust_dependencies()

    rust_register_toolchains(**kwargs)

_RUST_TOOLCHAIN_REPOSITORY_ATTRS = {
    "allocator_library": attr.string(
        doc = "Target that provides allocator functions when `rust_library` targets are embedded in a `cc_binary`.",
    ),
    "auth": attr.string_dict(
        doc = (
            "Auth object compatible with repository_ctx.download to use when downloading files. " +
            "See [repository_ctx.download](https://docs.bazel.build/versions/main/skylark/lib/repository_ctx.html#download) for more details."
        ),
    ),
    "auth_patterns": attr.string_list(
        doc = "A list of patterns to match against urls for which the auth object should be used.",
    ),
    "dev_components": attr.bool(
        doc = "Whether to download the rustc-dev components (defaults to False). Requires version to be \"nightly\".",
        default = False,
    ),
    "edition": attr.string(
        doc = (
            "The rust edition to be used by default (2015, 2018, or 2021). " +
            "If absent, every rule is required to specify its `edition` attribute."
        ),
    ),
    "exec_triple": attr.string(
        doc = "The Rust-style target that this compiler runs on",
        mandatory = True,
    ),
    "extra_exec_rustc_flags": attr.string_list(
        doc = "Extra flags to pass to rustc in exec configuration",
    ),
    "extra_rustc_flags": attr.string_list(
        doc = "Extra flags to pass to rustc in non-exec configuration",
    ),
    "global_allocator_library": attr.string(
        doc = "Target that provides allocator functions when a global allocator is used with [`cc_common.link`](https://bazel.build/rules/lib/toplevel/cc_common#link).",
    ),
    "netrc": attr.string(
        doc = ".netrc file to use for authentication; mirrors the eponymous attribute from http_archive",
    ),
    "opt_level": attr.string_dict(
        doc = "Rustc optimization levels. For more details see the documentation for `rust_toolchain.opt_level`.",
    ),
    "rustfmt_version": attr.string(
        doc = "The version of the tool among \"nightly\", \"beta\", or an exact version.",
    ),
    "sha256s": attr.string_dict(
        doc = "A dict associating tool subdirectories to sha256 hashes. See [rust_register_toolchains](#rust_register_toolchains) for more details.",
    ),
    "strip_level": attr.string_dict(
        doc = "Rustc strip levels. For more details see the documentation for `rust_toolchain.strip_level`.",
    ),
    "target_triple": attr.string(
        doc = "The Rust-style target that this compiler builds for.",
        mandatory = True,
    ),
    "urls": attr.string_list(
        doc = "A list of mirror urls containing the tools from the Rust-lang static file server. These must contain the '{}' used to substitute the tool being fetched (using .format).",
        default = DEFAULT_STATIC_RUST_URL_TEMPLATES,
    ),
    "version": attr.string(
        doc = "The version of the tool among \"nightly\", \"beta\", or an exact version.",
        mandatory = True,
    ),
}

def _include_llvm_tools(version, iso_date):
    """Rust 1.45.0 and nightly builds after 2020-05-22 need the llvm-tools gzip to get the libLLVM dylib"""
    if version in ("nightly", "beta"):
        if iso_date > "2020-05-22":
            return True
        return False

    version_semver = semver(version)
    if version_semver.major >= 1 and version_semver.minor >= 45:
        return True

    return False

def _rust_toolchain_tools_repository_impl(ctx):
    """The implementation of the rust toolchain tools repository rule."""
    sha256s = dict(ctx.attr.sha256s)
    iso_date = None
    version = ctx.attr.version
    version_array = version.split("/")
    if len(version_array) > 1:
        version = version_array[0]
        iso_date = version_array[1]

    check_version_valid(ctx.attr.version, iso_date)

    exec_triple = triple(ctx.attr.exec_triple)

    include_linker = True

    rustc_content, rustc_sha256 = load_rust_compiler(
        ctx = ctx,
        iso_date = iso_date,
        target_triple = exec_triple,
        version = version,
        include_linker = include_linker,
    )
    clippy_content, clippy_sha256 = load_clippy(
        ctx = ctx,
        iso_date = iso_date,
        target_triple = exec_triple,
        version = version,
    )
    cargo_content, cargo_sha256 = load_cargo(
        ctx = ctx,
        iso_date = iso_date,
        target_triple = exec_triple,
        version = version,
    )

    build_components = [
        rustc_content,
        clippy_content,
        cargo_content,
    ]
    sha256s.update(rustc_sha256 | clippy_sha256 | cargo_sha256)

    if ctx.attr.rustfmt_version:
        rustfmt_version = ctx.attr.rustfmt_version
        rustfmt_iso_date = None
        if rustfmt_version in ("nightly", "beta"):
            if iso_date:
                rustfmt_iso_date = iso_date
            else:
                fail("`rustfmt_version` does not include an iso_date. The following repository should either set `iso_date` or update `rustfmt_version` to include an iso_date suffix: {}".format(
                    ctx.name,
                ))
        elif rustfmt_version.startswith(("nightly", "beta")):
            rustfmt_version, _, rustfmt_iso_date = rustfmt_version.partition("/")
        rustfmt_content, rustfmt_sha256 = load_rustfmt(
            ctx = ctx,
            target_triple = triple(ctx.attr.exec_triple),
            version = rustfmt_version,
            iso_date = rustfmt_iso_date,
        )
        build_components.append(rustfmt_content)
        sha256s.update(rustfmt_sha256)

    include_llvm_tools = _include_llvm_tools(version, iso_date)
    if include_llvm_tools:
        llvm_tools_content, llvm_tools_sha256 = load_llvm_tools(
            ctx = ctx,
            target_triple = exec_triple,
            version = version,
            iso_date = iso_date,
        )
        build_components.append(llvm_tools_content)
        sha256s.update(llvm_tools_sha256)

    target_triple = triple(ctx.attr.target_triple)
    rust_stdlib_content, rust_stdlib_sha256 = load_rust_stdlib(
        ctx = ctx,
        target_triple = target_triple,
        version = version,
        iso_date = iso_date,
    )
    build_components.append(rust_stdlib_content)
    sha256s.update(rust_stdlib_sha256)

    stdlib_linkflags = None
    if "BAZEL_RUST_STDLIB_LINKFLAGS" in ctx.os.environ:
        stdlib_linkflags = ctx.os.environ["BAZEL_RUST_STDLIB_LINKFLAGS"].split(":")

    build_components.append(BUILD_for_rust_toolchain(
        name = "rust_toolchain",
        exec_triple = exec_triple,
        allocator_library = ctx.attr.allocator_library,
        global_allocator_library = ctx.attr.global_allocator_library,
        target_triple = target_triple,
        stdlib_linkflags = stdlib_linkflags,
        default_edition = ctx.attr.edition,
        include_rustfmt = not (not ctx.attr.rustfmt_version),
        include_llvm_tools = include_llvm_tools,
        include_linker = include_linker,
        extra_rustc_flags = ctx.attr.extra_rustc_flags,
        extra_exec_rustc_flags = ctx.attr.extra_exec_rustc_flags,
        opt_level = ctx.attr.opt_level if ctx.attr.opt_level else None,
        strip_level = ctx.attr.strip_level if ctx.attr.strip_level else None,
        version = ctx.attr.version,
    ))

    # Not all target triples are expected to have dev components
    if ctx.attr.dev_components:
        rustc_dev_sha256 = load_rustc_dev_nightly(
            ctx = ctx,
            target_triple = target_triple,
            version = version,
            iso_date = iso_date,
        )
        sha256s.update(rustc_dev_sha256)

    ctx.file("WORKSPACE.bazel", """workspace(name = "{}")""".format(
        ctx.name,
    ))
    ctx.file("BUILD.bazel", "\n".join(build_components))

    # Used to locate `rust_host_tools` repositories.
    ctx.file(ctx.name, "")

    repro = {"name": ctx.name}
    for key in _RUST_TOOLCHAIN_REPOSITORY_ATTRS:
        repro[key] = getattr(ctx.attr, key)
    repro["sha256s"] = sha256s

    # Bazel <8.3.0 lacks ctx.repo_metadata
    if not hasattr(ctx, "repo_metadata"):
        return repro

    reproducible = sha256s == dict(ctx.attr.sha256s)
    return ctx.repo_metadata(
        reproducible = reproducible,
        attrs_for_reproducibility = {} if reproducible else repro,
    )

rust_toolchain_tools_repository = repository_rule(
    doc = (
        "Composes a single workspace containing the toolchain components for compiling on a given " +
        "platform to a series of target platforms.\n" +
        "\n" +
        "A given instance of this rule should be accompanied by a toolchain_repository_proxy " +
        "invocation to declare its toolchains to Bazel; the indirection allows separating toolchain " +
        "selection from toolchain fetching."
    ),
    attrs = _RUST_TOOLCHAIN_REPOSITORY_ATTRS,
    implementation = _rust_toolchain_tools_repository_impl,
)

def _toolchain_repository_proxy_impl(repository_ctx):
    repository_ctx.file("WORKSPACE.bazel", """workspace(name = "{}")""".format(
        repository_ctx.name,
    ))

    repository_ctx.file("BUILD.bazel", BUILD_for_toolchain(
        name = "toolchain",
        toolchain = repository_ctx.attr.toolchain,
        target_settings = repository_ctx.attr.target_settings,
        toolchain_type = repository_ctx.attr.toolchain_type,
        target_compatible_with = repository_ctx.attr.target_compatible_with,
        exec_compatible_with = repository_ctx.attr.exec_compatible_with,
    ))

toolchain_repository_proxy = repository_rule(
    doc = (
        "Generates a toolchain-bearing repository that declares the toolchains from some other " +
        "rust_toolchain_repository."
    ),
    attrs = {
        "exec_compatible_with": attr.string_list(
            doc = "A list of constraints for the execution platform for this toolchain.",
        ),
        "target_compatible_with": attr.string_list(
            doc = "A list of constraints for the target platform for this toolchain.",
        ),
        "target_settings": attr.string_list(
            doc = "A list of config_settings that must be satisfied by the target configuration in order for this toolchain to be selected during toolchain resolution.",
        ),
        "toolchain": attr.string(
            doc = "The name of the toolchain implementation target.",
            mandatory = True,
        ),
        "toolchain_type": attr.string(
            doc = "The toolchain type of the toolchain to declare",
            mandatory = True,
        ),
    },
    implementation = _toolchain_repository_proxy_impl,
)

# For legacy support
rust_toolchain_repository_proxy = toolchain_repository_proxy

# N.B. A "proxy repository" is needed to allow for registering the toolchain (with constraints)
# without actually downloading the toolchain.
def rust_toolchain_repository(
        name,
        version,
        exec_triple,
        target_triple,
        exec_compatible_with = None,
        target_compatible_with = None,
        target_settings = [],
        channel = None,
        allocator_library = None,
        global_allocator_library = None,
        rustfmt_version = None,
        edition = None,
        dev_components = False,
        extra_rustc_flags = None,
        extra_exec_rustc_flags = None,
        opt_level = None,
        strip_level = None,
        sha256s = None,
        urls = DEFAULT_STATIC_RUST_URL_TEMPLATES,
        auth = None,
        netrc = None,
        auth_patterns = None):
    """Assembles a remote repository for the given toolchain params, produces a proxy repository \
    to contain the toolchain declaration, and registers the toolchains.

    Args:
        name (str): The name of the generated repository
        version (str): The version of the tool among "nightly", "beta", or an exact version.
        exec_triple (str): The Rust-style target that this compiler runs on.
        target_triple (str): The Rust-style target to build for.
        channel (str, optional): The channel of the Rust toolchain.
        exec_compatible_with (list, optional): A list of constraints for the execution platform for this toolchain.
        target_compatible_with (list, optional): A list of constraints for the target platform for this toolchain.
        target_settings (list of labels as strings, optional): A list of config_settings that must be satisfied by the target configuration in order for this toolchain to be selected during toolchain resolution.
        allocator_library (str, optional): Target that provides allocator functions when rust_library targets are embedded in a cc_binary.
        global_allocator_library (str, optional): Target that provides allocator functions when a global allocator is used with cc_common.link.
        rustfmt_version (str, optional):  The version of rustfmt to be associated with the
            toolchain.
        edition (str, optional): The rust edition to be used by default (2015, 2018, or 2021). If absent, every rule is required to specify its `edition` attribute.
        dev_components (bool, optional): Whether to download the rustc-dev components.
            Requires version to be "nightly". Defaults to False.
        extra_rustc_flags (list, optional): Extra flags to pass to rustc in non-exec configuration.
            Subject to Make variable expansion with respect to RUST_SYSROOT,
            RUST_SYSROOT_SHORT, RUSTC, etc.
        extra_exec_rustc_flags (list, optional): Extra flags to pass to rustc in exec configuration.
            Subject to Make variable expansion with respect to RUST_SYSROOT,
            RUST_SYSROOT_SHORT, RUSTC, etc.
        opt_level (dict, optional): Optimization level config for this toolchain.
        strip_level (dict, optional): Strip level config for this toolchain.
        sha256s (str, optional): A dict associating tool subdirectories to sha256 hashes. See
            [rust_register_toolchains](#rust_register_toolchains) for more details.
        urls (list, optional): A list of mirror urls containing the tools from the Rust-lang static file server. These must contain the '{}' used to substitute the tool being fetched (using .format). Defaults to ['https://static.rust-lang.org/dist/{}.tar.xz']
        auth (dict): Auth object compatible with repository_ctx.download to use when downloading files.
            See [repository_ctx.download](https://docs.bazel.build/versions/main/skylark/lib/repository_ctx.html#download) for more details.
        netrc (str, optional): .netrc file to use for authentication; mirrors the eponymous attribute from http_archive
        auth_patterns (list, optional): A list of patterns to match against urls for which the auth object should be used.

    Returns:
        dict[str, str]: Information about the registerable toolchain created by this rule.
    """
    if exec_compatible_with == None:
        exec_compatible_with = triple_to_constraint_set(exec_triple)

    if target_compatible_with == None:
        target_compatible_with = triple_to_constraint_set(target_triple)

    tools_repo_name = "{}_tools".format(name)

    rust_toolchain_tools_repository(
        name = tools_repo_name,
        exec_triple = exec_triple,
        allocator_library = allocator_library,
        global_allocator_library = global_allocator_library,
        target_triple = target_triple,
        version = version,
        rustfmt_version = rustfmt_version,
        edition = edition,
        dev_components = dev_components,
        extra_rustc_flags = extra_rustc_flags,
        extra_exec_rustc_flags = extra_exec_rustc_flags,
        opt_level = opt_level,
        strip_level = strip_level,
        sha256s = sha256s,
        urls = urls,
        auth = auth,
        netrc = netrc,
        auth_patterns = auth_patterns,
    )

    channel_target_settings = ["@rules_rust//rust/toolchain/channel:{}".format(channel)] if channel else []

    tools_toolchain_label = "@{}//:rust_toolchain".format(tools_repo_name)

    toolchain_type = "@rules_rust//rust:toolchain"

    toolchain_repository_proxy(
        name = name,
        toolchain = tools_toolchain_label,
        target_settings = channel_target_settings + target_settings,
        toolchain_type = toolchain_type,
        exec_compatible_with = exec_compatible_with,
        target_compatible_with = target_compatible_with,
    )

    return {
        "exec_compatible_with": exec_compatible_with,
        "name": name,
        "target_compatible_with": target_compatible_with,
        "target_settings": target_settings,
        "toolchain_label": "@{name}//:toolchain".format(name = name),
        "toolchain_type": toolchain_type,
        "tools_toolchain_label": tools_toolchain_label,
    }

_RUST_ANALYZER_TOOLCHAIN_TOOLS_REPOSITORY_ATTRS = {
    "auth": attr.string_dict(
        doc = (
            "Auth object compatible with repository_ctx.download to use when downloading files. " +
            "See [repository_ctx.download](https://docs.bazel.build/versions/main/skylark/lib/repository_ctx.html#download) for more details."
        ),
    ),
    "auth_patterns": attr.string_list(
        doc = "A list of patterns to match against urls for which the auth object should be used.",
    ),
    "netrc": attr.string(
        doc = ".netrc file to use for authentication; mirrors the eponymous attribute from http_archive",
    ),
    "sha256s": attr.string_dict(
        doc = "A dict associating tool subdirectories to sha256 hashes. See [rust_register_toolchains](#rust_register_toolchains) for more details.",
    ),
    "urls": attr.string_list(
        doc = "A list of mirror urls containing the tools from the Rust-lang static file server. These must contain the '{}' used to substitute the tool being fetched (using .format).",
        default = DEFAULT_STATIC_RUST_URL_TEMPLATES,
    ),
    "version": attr.string(
        doc = "The version of the tool among \"nightly\", \"beta\" with an iso date, or an exact version.",
        mandatory = True,
    ),
}

def _rust_analyzer_toolchain_tools_repository_impl(repository_ctx):
    sha256s = dict(repository_ctx.attr.sha256s)
    include_linker = True

    iso_date = None
    version = repository_ctx.attr.version
    version_array = version.split("/")
    if len(version_array) > 1:
        version = version_array[0]
        iso_date = version_array[1]

    sha256s.update(load_rust_src(
        ctx = repository_ctx,
        iso_date = iso_date,
        version = version,
    ))

    repository_ctx.file("WORKSPACE.bazel", """workspace(name = "{}")""".format(
        repository_ctx.name,
    ))

    host_triple = get_host_triple(repository_ctx)
    rustc_content, rustc_sha256 = load_rust_compiler(
        ctx = repository_ctx,
        iso_date = iso_date,
        target_triple = host_triple,
        version = version,
        include_linker = include_linker,
    )
    build_contents = [rustc_content]
    sha256s.update(rustc_sha256)
    rustc = "//:rustc"

    proc_macro_srv = None
    if includes_rust_analyzer_proc_macro_srv(version, iso_date):
        build_contents.append(BUILD_for_rust_analyzer_proc_macro_srv(host_triple))
        proc_macro_srv = "//:rust_analyzer_proc_macro_srv"

    # Load rust-analyzer binary from official Rust distribution
    rust_analyzer = None
    rust_analyzer_content, rust_analyzer_sha256 = load_rust_analyzer(
        ctx = repository_ctx,
        target_triple = host_triple,
        version = version,
        iso_date = iso_date,
    )
    if rust_analyzer_content:
        build_contents.append(rust_analyzer_content)
        sha256s.update(rust_analyzer_sha256)
        rust_analyzer = "//:rust_analyzer"

    build_contents.append(BUILD_for_rust_analyzer_toolchain(
        name = "rust_analyzer_toolchain",
        rustc = rustc,
        proc_macro_srv = proc_macro_srv,
        rust_analyzer = rust_analyzer,
    ))

    repository_ctx.file("BUILD.bazel", "\n".join(build_contents))
    repository_ctx.file("WORKSPACE.bazel", """workspace(name = "{}")""".format(
        repository_ctx.name,
    ))

    repro = {"name": repository_ctx.name}
    for key in _RUST_ANALYZER_TOOLCHAIN_TOOLS_REPOSITORY_ATTRS:
        repro[key] = getattr(repository_ctx.attr, key)
    repro["sha256s"] = sha256s

    # Bazel <8.3.0 lacks ctx.repo_metadata
    if not hasattr(repository_ctx, "repo_metadata"):
        return repro

    reproducible = sha256s == dict(repository_ctx.attr.sha256s)
    return repository_ctx.repo_metadata(
        reproducible = reproducible,
        attrs_for_reproducibility = {} if reproducible else repro,
    )

rust_analyzer_toolchain_tools_repository = repository_rule(
    doc = "A repository rule for defining a rust_analyzer_toolchain with a `rust-src` artifact.",
    implementation = _rust_analyzer_toolchain_tools_repository_impl,
    attrs = _RUST_ANALYZER_TOOLCHAIN_TOOLS_REPOSITORY_ATTRS,
)

def rust_analyzer_toolchain_repository(
        name,
        version,
        exec_compatible_with = [],
        target_compatible_with = [],
        sha256s = None,
        urls = None,
        auth = None,
        netrc = None,
        auth_patterns = None):
    """Assemble a remote rust_analyzer_toolchain target based on the given params.

    Args:
        name (str): The name of the toolchain proxy repository containing the registerable toolchain.
        version (str): The version of the tool among "nightly", "beta', or an exact version.
        exec_compatible_with (list, optional): A list of constraints for the execution platform for this toolchain.
        target_compatible_with (list, optional): A list of constraints for the target platform for this toolchain.
        sha256s (str, optional): A dict associating tool subdirectories to sha256 hashes. See
            [rust_register_toolchains](#rust_register_toolchains) for more details.
        urls (list, optional): A list of mirror urls containing the tools from the Rust-lang static file server. These must contain the '{}' used to substitute the tool being fetched (using .format). Defaults to ['https://static.rust-lang.org/dist/{}.tar.xz']
        auth (dict): Auth object compatible with repository_ctx.download to use when downloading files.
            See [repository_ctx.download](https://docs.bazel.build/versions/main/skylark/lib/repository_ctx.html#download) for more details.
        netrc (str, optional): .netrc file to use for authentication; mirrors the eponymous attribute from http_archive
        auth_patterns (dict, optional): Override mapping of hostnames to authorization patterns; mirrors the eponymous attribute from http_archive

    Returns:
        str: The name of a registerable rust_analyzer_toolchain.
    """
    rust_analyzer_toolchain_tools_repository(
        name = name + "_tools",
        version = version,
        sha256s = sha256s,
        urls = urls,
        auth = auth,
        netrc = netrc,
        auth_patterns = auth_patterns,
    )

    toolchain_repository_proxy(
        name = name,
        toolchain = "@{}//:{}".format(name + "_tools", "rust_analyzer_toolchain"),
        toolchain_type = "@rules_rust//rust/rust_analyzer:toolchain_type",
        exec_compatible_with = exec_compatible_with,
        target_compatible_with = target_compatible_with,
    )

    return "@{}//:toolchain".format(
        name,
    )

_RUSTFMT_TOOLCHAIN_TOOLS_ATTRS = {
    "auth": attr.string_dict(
        doc = (
            "Auth object compatible with repository_ctx.download to use when downloading files. " +
            "See [repository_ctx.download](https://docs.bazel.build/versions/main/skylark/lib/repository_ctx.html#download) for more details."
        ),
    ),
    "auth_patterns": attr.string_dict(
        doc = "Override mapping of hostnames to authorization patterns; mirrors the eponymous attribute from http_archive",
    ),
    "exec_triple": attr.string(
        doc = "The Rust-style triple Rustfmt is expected to run on.",
        mandatory = True,
    ),
    "netrc": attr.string(
        doc = ".netrc file to use for authentication; mirrors the eponymous attribute from http_archive",
    ),
    "sha256s": attr.string_dict(
        doc = "A dict associating tool subdirectories to sha256 hashes. See [rust_register_toolchains](#rust_register_toolchains) for more details.",
    ),
    "urls": attr.string_list(
        doc = "A list of mirror urls containing the tools from the Rust-lang static file server. These must contain the '{}' used to substitute the tool being fetched (using .format).",
        default = DEFAULT_STATIC_RUST_URL_TEMPLATES,
    ),
    "version": attr.string(
        doc = "The version of the tool among \"nightly\", \"beta\" with an iso date, or an exact version.",
        mandatory = True,
    ),
}

def _rustfmt_toolchain_tools_repository_impl(repository_ctx):
    sha256s = dict(repository_ctx.attr.sha256s)
    repository_ctx.file("WORKSPACE.bazel", """workspace(name = "{}")""".format(
        repository_ctx.name,
    ))

    include_linker = True

    iso_date = None
    version = repository_ctx.attr.version
    version_array = version.split("/")
    if len(version_array) > 1:
        version = version_array[0]
        iso_date = version_array[1]

    rustfmt = "//:rustfmt_bin"
    rustc = "//:rustc"
    rustc_lib = "//:rustc_lib"

    exec_triple = triple(repository_ctx.attr.exec_triple)

    rustc_content, rustc_sha256 = load_rust_compiler(
        ctx = repository_ctx,
        iso_date = iso_date,
        target_triple = exec_triple,
        version = version,
        include_linker = include_linker,
    )
    rustfmt_content, rustfmt_sha256 = load_rustfmt(
        ctx = repository_ctx,
        iso_date = iso_date,
        target_triple = exec_triple,
        version = version,
    )

    build_contents = [
        rustc_content,
        rustfmt_content,
        BUILD_for_rustfmt_toolchain(
            name = "rustfmt_toolchain",
            rustfmt = rustfmt,
            rustc = rustc,
            rustc_lib = rustc_lib,
        ),
    ]
    sha256s.update(rustc_sha256 | rustfmt_sha256)

    repository_ctx.file("BUILD.bazel", "\n".join(build_contents))
    repository_ctx.file("WORKSPACE.bazel", """workspace(name = "{}")""".format(
        repository_ctx.name,
    ))

    repro = {"name": repository_ctx.name}
    for key in _RUSTFMT_TOOLCHAIN_TOOLS_ATTRS:
        repro[key] = getattr(repository_ctx.attr, key)
    repro["sha256s"] = sha256s

    # Bazel <8.3.0 lacks ctx.repo_metadata
    if not hasattr(repository_ctx, "repo_metadata"):
        return repro

    reproducible = sha256s == dict(repository_ctx.attr.sha256s)
    return repository_ctx.repo_metadata(
        reproducible = reproducible,
        attrs_for_reproducibility = {} if reproducible else repro,
    )

rustfmt_toolchain_tools_repository = repository_rule(
    doc = "A repository rule for defining a rustfmt_toolchain.",
    attrs = _RUSTFMT_TOOLCHAIN_TOOLS_ATTRS,
    implementation = _rustfmt_toolchain_tools_repository_impl,
)

def rustfmt_toolchain_repository(
        name,
        version,
        exec_triple,
        exec_compatible_with = None,
        target_compatible_with = None,
        channel = None,
        sha256s = None,
        urls = None,
        auth = None,
        netrc = None,
        auth_patterns = None):
    """Assemble a remote rustfmt_toolchain target based on the given params.

    Args:
        name (str): The name of the toolchain proxy repository containing the registerable toolchain.
        version (str): The version of the tool among "nightly", "beta', or an exact version.
        exec_triple (str): The platform triple Rustfmt is expected to run on.
        exec_compatible_with (list, optional): A list of constraints for the execution platform for this toolchain.
        target_compatible_with (list, optional): A list of constraints for the target platform for this toolchain.
        channel (str, optional): The channel value to with which to constrain the toolchain.
        sha256s (str, optional): A dict associating tool subdirectories to sha256 hashes. See
            [rust_register_toolchains](#rust_register_toolchains) for more details.
        urls (list, optional): A list of mirror urls containing the tools from the Rust-lang static file server. These must contain the '{}' used to substitute the tool being fetched (using .format). Defaults to ['https://static.rust-lang.org/dist/{}.tar.xz']
        auth (dict): Auth object compatible with repository_ctx.download to use when downloading files.
            See [repository_ctx.download](https://docs.bazel.build/versions/main/skylark/lib/repository_ctx.html#download) for more details.
        netrc (str, optional): .netrc file to use for authentication; mirrors the eponymous attribute from http_archive
        auth_patterns (dict, optional): Override mapping of hostnames to authorization patterns; mirrors the eponymous attribute from http_archive

    Returns:
        str: The name of a registerable rustfmt_toolchain.
    """
    if exec_compatible_with == None:
        exec_compatible_with = triple_to_constraint_set(exec_triple)

    rustfmt_toolchain_tools_repository(
        name = name + "_tools",
        version = version,
        sha256s = sha256s,
        urls = urls,
        auth = auth,
        netrc = netrc,
        auth_patterns = auth_patterns,
        exec_triple = exec_triple,
    )

    toolchain_repository_proxy(
        name = name,
        toolchain = "@{}//:{}".format(name + "_tools", "rustfmt_toolchain"),
        toolchain_type = "@rules_rust//rust/rustfmt:toolchain_type",
        target_settings = ["@rules_rust//rust/toolchain/channel:{}".format(channel)] if channel else None,
        exec_compatible_with = exec_compatible_with,
        target_compatible_with = target_compatible_with,
    )

    return "@{}//:toolchain".format(
        name,
    )

def _rust_toolchain_set_repository_impl(repository_ctx):
    repository_ctx.file("WORKSPACE.bazel", """workspace(name = "{}")""".format(
        repository_ctx.name,
    ))

    repository_ctx.file("BUILD.bazel", """exports_files(["defs.bzl"])""")
    repository_ctx.file("defs.bzl", "ALL_TOOLCHAINS = {}\n".format(
        json.encode_indent(repository_ctx.attr.toolchains, indent = " " * 4),
    ))

rust_toolchain_set_repository = repository_rule(
    doc = (
        "Generates a toolchain-bearing repository that declares the toolchains from some other " +
        "rust_toolchain_repository."
    ),
    attrs = {
        "toolchains": attr.string_list(
            doc = "The list of all toolchains created by the current `rust_toolchain_set`",
            mandatory = True,
        ),
    },
    implementation = _rust_toolchain_set_repository_impl,
)

def _get_toolchain_repositories(
        *,
        name,
        exec_triple,
        extra_target_triples,
        versions,
        fallback_target_compatible_with,
        compact_windows_names,
        aliases):
    """Collect structs represent toolchain repositories matching the given parameters.

    Args:
        name (str): The base name to use for toolchains.
        exec_triple (triple): The execution triple associated with the toolchain.
        extra_target_triples (list[triple]): Additional target triples to get toolchains for.
            the `exec_triple` is a default `target_triple`.
        versions (str): The version of rustc to use.
        fallback_target_compatible_with (list): _description_
        compact_windows_names (bool): Whether or not to produce compact repository names for windows
            toolchains. This is to avoid MAX_PATH issues.
        aliases (dict): Replacement names to use for toolchains created by this macro.

    Returns:
        list[struct]: A list of toolchain structs
            - name: The name of the toolchain repository.
            - target_triple: The target triple of the toolchain.
            - channel: The toolchain channel (nightly/stable).
            - target_constraints: Bazel constraints associated with the toolchain.
    """
    extra_target_triples_list = extra_target_triples.keys() if type(extra_target_triples) == "dict" else extra_target_triples

    toolchain_repos = {}

    for target_triple in depset([exec_triple] + extra_target_triples_list).to_list():
        # Parse all provided versions while checking for duplicates
        channels = {}
        for version in versions:
            if version.startswith(("beta", "nightly")):
                channel, _, _ = version.partition("/")
            else:
                channel = "stable"

            if channel in channels:
                fail("Duplicate {} channels provided for {}: {}".format(channel, name, versions))

            channels.update({channel: struct(
                name = channel,
                version = version,
            )})

        if type(extra_target_triples) == "dict" and target_triple in extra_target_triples:
            target_constraints = extra_target_triples[target_triple]
        elif fallback_target_compatible_with != None:
            target_constraints = fallback_target_compatible_with
        else:
            target_constraints = triple_to_constraint_set(target_triple)

        # Define toolchains for each requested version
        for channel in channels.values():
            # Check if this toolchain is requested to be aliased.
            full_name = "{}__{}__{}".format(name, target_triple, channel.name)
            if full_name in aliases:
                full_name = aliases.pop(full_name)
            elif compact_windows_names and "windows" in exec_triple:
                full_name = "rw-{}".format(abs(hash(full_name)))

            toolchain_repo = struct(
                name = full_name,
                target_triple = target_triple,
                channel = channel,
                target_constraints = target_constraints,
            )

            if full_name in toolchain_repos:
                fail("Duplicate toolchain name of {} found in Rust toolchain repositories:\n{}\n{}".format(
                    full_name,
                    toolchain_repos[full_name],
                    toolchain_repo,
                ))

            toolchain_repos[full_name] = toolchain_repo

    return toolchain_repos.values()

def _get_flags_for_triple(name, flags, target_triple):
    """Infer toolchain-specific flags depending on the type (list, dict, optional)."""
    if flags == None:
        return None
    elif type(flags) == "list":
        return flags
    elif type(flags) == "dict":
        return flags.get(target_triple)
    else:
        fail(name + " should be a list or a dict")

def rust_repository_set(
        *,
        name,
        versions,
        exec_triple,
        target_settings = [],
        allocator_library = None,
        global_allocator_library = None,
        extra_target_triples = {},
        rustfmt_version = None,
        edition = None,
        dev_components = False,
        extra_rustc_flags = None,
        extra_exec_rustc_flags = None,
        opt_level = None,
        strip_level = None,
        sha256s = None,
        urls = DEFAULT_STATIC_RUST_URL_TEMPLATES,
        auth = None,
        netrc = None,
        auth_patterns = None,
        register_toolchain = True,
        exec_compatible_with = None,
        default_target_compatible_with = None,
        aliases = {},
        compact_windows_names = _COMPACT_WINDOWS_NAMES):
    """Assembles a remote repository for the given toolchain params, produces a proxy repository \
    to contain the toolchain declaration, and registers the toolchains.

    Args:
        name (str): The name of the generated repository
        versions (list, optional): A list of toolchain versions to download. This parameter only accepts one versions
            per channel. E.g. `["1.65.0", "nightly/2022-11-02", "beta/2020-12-30"]`.
        exec_triple (str): The Rust-style target that this compiler runs on
        target_settings (list of labels as strings, optional): A list of config_settings that must be satisfied by the target configuration in order for this set of toolchains to be selected during toolchain resolution.
        allocator_library (str, optional): Target that provides allocator functions when rust_library targets are
            embedded in a cc_binary.
        global_allocator_library (str, optional): Target that provides allocator functions a global allocator is used with cc_common.link.
        extra_target_triples (list or map, optional): Additional rust-style targets that this set of
            toolchains should support. If a map, values should be (optional) target_compatible_with lists for that particular target triple.
        rustfmt_version (str, optional):  The version of rustfmt to be associated with the
            toolchain.
        edition (str, optional): The rust edition to be used by default (2015, 2018, or 2021). If absent, every rule is
            required to specify its `edition` attribute.
        dev_components (bool, optional): Whether to download the rustc-dev components.
            Requires version to be "nightly".
        extra_rustc_flags (dict, list, optional): Dictionary of target triples to list of extra flags to pass to rustc in non-exec configuration.
        extra_exec_rustc_flags (dict, list, optional): Dictionary of target triples to list of extra flags to pass to rustc in exec configuration.
        opt_level (dict, dict, optional): Dictionary of target triples to optimization config.
        strip_level (dict, dict, optional): Dictionary of target triples to strip config.
        sha256s (str, optional): A dict associating tool subdirectories to sha256 hashes. See
            [rust_register_toolchains](#rust_register_toolchains) for more details.
        urls (list, optional): A list of mirror urls containing the tools from the Rust-lang static file server. These
            must contain the '{}' used to substitute the tool being fetched (using .format).
        auth (dict): Auth object compatible with repository_ctx.download to use when downloading files.
            See [repository_ctx.download](https://docs.bazel.build/versions/main/skylark/lib/repository_ctx.html#download) for more details.
        netrc (str, optional): .netrc file to use for authentication; mirrors the eponymous attribute from http_archive
        auth_patterns (dict, optional): Override mapping of hostnames to authorization patterns; mirrors the eponymous attribute from http_archive

        register_toolchain (bool): If True, the generated `rust_toolchain` target will become a registered toolchain.
        exec_compatible_with (list, optional): A list of constraints for the execution platform for this toolchain.
        default_target_compatible_with (list, optional): A list of constraints for the target platform for this toolchain when the exec platform is the same as the target platform.
        aliases (dict): Replacement names to use for toolchains created by this macro.
        compact_windows_names (bool): Whether or not to produce compact repository names for windows
            toolchains. This is to avoid MAX_PATH issues.

    Returns:
        dict[str, dict]: A dict of information about all generated toolchains.
    """

    all_toolchain_details = {}
    toolchain_labels = []
    for toolchain in _get_toolchain_repositories(
        name = name,
        exec_triple = exec_triple,
        extra_target_triples = extra_target_triples,
        versions = versions,
        fallback_target_compatible_with = default_target_compatible_with,
        aliases = aliases,
        compact_windows_names = compact_windows_names,
    ):
        toolchain_extra_exec_rustc_flags = _get_flags_for_triple(
            "extra_exec_rustc_flags",
            extra_exec_rustc_flags,
            toolchain.target_triple,
        )
        toolchain_extra_rustc_flags = _get_flags_for_triple(
            "extra_rustc_flags",
            extra_rustc_flags,
            toolchain.target_triple,
        )

        toolchain_info = rust_toolchain_repository(
            name = toolchain.name,
            allocator_library = allocator_library,
            global_allocator_library = global_allocator_library,
            auth = auth,
            netrc = netrc,
            auth_patterns = auth_patterns,
            channel = toolchain.channel.name,
            dev_components = dev_components,
            edition = edition,
            exec_triple = exec_triple,
            extra_exec_rustc_flags = toolchain_extra_exec_rustc_flags,
            extra_rustc_flags = toolchain_extra_rustc_flags,
            opt_level = opt_level.get(toolchain.target_triple) if opt_level != None else None,
            strip_level = strip_level.get(toolchain.target_triple) if strip_level != None else None,
            target_settings = target_settings,
            rustfmt_version = rustfmt_version,
            sha256s = sha256s,
            target_triple = toolchain.target_triple,
            urls = urls,
            version = toolchain.channel.version,
            exec_compatible_with = exec_compatible_with,
            target_compatible_with = toolchain.target_constraints,
        )
        toolchain_labels.append(toolchain_info["toolchain_label"])
        all_toolchain_details[toolchain.name] = toolchain_info

    # This repository exists to allow `rust_repository_set` to work with the `maybe` wrapper.
    rust_toolchain_set_repository(
        name = name,
        toolchains = toolchain_labels,
    )

    # Register toolchains
    if register_toolchain:
        native.register_toolchains(*toolchain_labels)
        native.register_toolchains(str(Label("//rust/private/dummy_cc_toolchain:dummy_cc_wasm32_toolchain")))
        native.register_toolchains(str(Label("//rust/private/dummy_cc_toolchain:dummy_cc_wasm64_toolchain")))

    return all_toolchain_details
