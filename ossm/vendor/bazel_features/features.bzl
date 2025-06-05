"""Defines all the features this module supports detecting."""

load("@bazel_features_globals//:globals.bzl", "globals")
load("//private:util.bzl", "ge", "ge_same_major", "lt")

_cc = struct(
    # Whether @bazel_tools//tools/cpp:optional_current_cc_toolchain and the `mandatory` parameter
    # on find_cpp_toolchain are available (#17308).
    # Note: While the target and parameter are available in 6.1.0, they only take effect in Bazel 7.
    find_cpp_toolchain_has_mandatory_param = ge("6.1.0"),
    # Whether `dsym_path` is no longer incorrectly quoted
    # https://github.com/bazelbuild/bazel/commit/7a04b220f30b92d11049157279ef0cfb5130870d
    fixed_dsym_path_quoting = ge_same_major("7.2.0") or ge("8.0.0-pre.20240404.3"),
    # Note: In Bazel 6.3 the `grep_includes` parameter is optional and a no-op in the cc_common API
    # In future Bazel versions it will be removed altogether.
    grep_includes_is_optional = ge("6.3.0"),
    # From 7.0.0-pre.20230724.1 on `ObjcProvider` no longer contains linking info
    # https://github.com/bazelbuild/bazel/commit/426f2254669f62b7d332094a0af6d4dc6200ad51
    objc_linking_info_migrated = ge("7.0.0-pre.20230724.1"),
    # https://github.com/bazelbuild/bazel/commit/c8c3878088cb706b820d506a682e1156b7e8c64d
    swift_fragment_removed = ge("8.0.0-pre.20240101.1"),
    # Whether the Unix C/C++ toolchain passes -undefined dynamic_lookup to the
    # macOS linker.  Added in commit
    # https://github.com/bazelbuild/bazel/commit/314cf1f9e4b332955c4800b2451db4e926c3e092
    # and removed again in commit
    # https://github.com/bazelbuild/bazel/commit/4853dfd02ac7440a04caada830b7b61b6081bdfe.
    undefined_dynamic_lookup = ge("0.25.0") and lt("7.0.0-pre.20230118.2"),
    # Whether the treat_warnings_as_errors feature works on macOS.
    # https://github.com/bazelbuild/bazel/commit/3d7c5ae47e2a02ccd81eb8024f22d56ae7811c9b
    treat_warnings_as_errors_works_on_macos = ge("7.1.0"),
    # Whether protobuf repository can access private C++ features
    # https://github.com/bazelbuild/bazel/commit/6022ee81705295704dcbedb2ceb5869049191121
    protobuf_on_allowlist = ge("8.0.0"),
    # Whether cc_{binary,test} can be extended.
    # https://github.com/bazelbuild/bazel/commit/b746d663da71f937390809f0e8368112cafafb56
    rules_support_extension = ge("8.0.1"),
)

_docs = struct(
    # The stardoc output changed in https://github.com/bazelbuild/bazel/commit/bd1c3af2ea14e81268e940d2b8ba5ad00c3f08d7
    # This may be required for "diff tests" that assert on the generated API docs.
    kwargs_name_with_double_star = ge("8.0.0-pre.20240603.2"),
    # Starting with Bazel 8.1.0, all strings exported to Stardoc (docstrings,
    # rule names, etc.) are interpreted as UTF-8.  Previously, they were
    # interpreted as Latin-1, resulting in double-encoding if the underlying
    # Starlark file was actually UTF-8-encoded.  See
    # https://github.com/bazelbuild/bazel/pull/24935.
    utf8_enabled = ge("8.1.0"),
)

_external_deps = struct(
    # Whether --enable_bzlmod is set, and thus, whether str(Label(...)) produces canonical label
    # literals (i.e., "@@repo//pkg:file").
    is_bzlmod_enabled = str(Label("//:invalid")).startswith("@@"),
    # Whether module_extension has the os_dependent and arch_dependent parameters.
    # https://github.com/bazelbuild/bazel/commit/970b9dda7cd215a29d73a53871500bc4e2dc6142
    module_extension_has_os_arch_dependent = ge("6.4.0"),
    # Whether repository_ctx#download has the block parameter, allowing parallel downloads (#19674)
    download_has_block_param = ge("7.1.0"),
    # Whether repository_ctx#download has the headers parameter, allowing arbitrary headers (#17829)
    download_has_headers_param = ge("7.1.0"),
    # Whether repository_ctx#extract has unicode filename extraction fix (#18448)
    extract_supports_unicode_filenames = ge("6.4.0"),
    # Whether the `bazel mod tidy` subcommand is available (#19674)
    # https://github.com/bazelbuild/bazel/commit/9f0f23211293589d812cb9ea4aaaead52486c52e
    # https://github.com/bazelbuild/bazel/commit/9fe80d33e129de521b696c330802aad9782db18f
    bazel_mod_tidy = ge_same_major("7.1.0") or ge("8.0.0-pre.20240213.1"),
    # Whether module_ctx.extension_metadata has the reproducible parameter (#19674)
    # https://github.com/bazelbuild/bazel/commit/c796aba6ee36970956ea32b46a2f121bb4d1818a
    # https://github.com/bazelbuild/bazel/commit/e730201e6bf8d6c1c80433b5b42305c3167a8660
    extension_metadata_has_reproducible = ge_same_major("7.1.0") or ge("8.0.0-pre.20240213.1"),
    # Whether repository_ctx#getenv exists (#19511)
    # Note: This primarily targets conditionally adding environ
    # attributes to repository rule declarations.  Inside repository rule
    # implementations, consider using the simpler and more descriptive
    # hasattr(repository_ctx, "getenv") as an alternative.
    # https://github.com/bazelbuild/bazel/commit/c230e39fb225edd206ed0aa07cfcdd8c51589965
    # https://github.com/bazelbuild/bazel/commit/25815511434d17f2843f73e0ff5231f3d80bc44e
    repository_ctx_has_getenv = ge_same_major("7.1.0") or ge("8.0.0-pre.20240128.3"),
)

_flags = struct(
    # This flag was renamed in https://github.com/bazelbuild/bazel/pull/18313
    allow_unresolved_symlinks = (
        "allow_unresolved_symlinks" if ge("7.0.0-pre.20230628.2") else "experimental_allow_unresolved_symlinks"
    ),
)

_java = struct(
    # Whether the JavaInfo constructor has add_exports/add_opens named parameters. Added in
    # https://github.com/bazelbuild/bazel/commit/d2783a3c3d1b899beb674e029bfea3519062e8be (HEAD)
    # https://github.com/bazelbuild/bazel/commit/e2249f91ff84541565d8ba841592a0a8a43fcb66 (7.0.0)
    java_info_constructor_module_flags = ge_same_major("7.0.0") or ge("8.0.0-pre.20240101.1"),
)

_proto = struct(
    # Bazel 7.0.0 introduced ProtoInfo in Starlark, which can be constructed and has different fields
    # than ProtoInfo in previous versions. The check is needed for proto rules that are using ProtoInfo from Bazel.
    starlark_proto_info = ge("7.0.0"),
)

_rules = struct(
    # Whether runfiles may contain all characters. Support for all characters added in:
    # https://github.com/bazelbuild/bazel/commit/c9115305cb81e7fe645f91ca790642cab136b2a1
    all_characters_allowed_in_runfiles = ge("7.4.0"),
    # Whether the computed_substitutions parameter of ctx.actions.expand_template and ctx.actions.template_dict are stable.
    # https://github.com/bazelbuild/bazel/commit/61c31d255b6ba65c372253f65043d6ea3f10e1f9
    expand_template_has_computed_substitutions = ge("7.0.0-pre.20231011.2"),
    # Whether TemplateDict#add_joined allows the map_each callback to return a list of strings (#17306)
    template_dict_map_each_can_return_list = ge("6.1.0"),
    # Whether coverage_common.instrumented_files_info spports the
    # metadata_files parameter.  Introduced in commit
    # https://github.com/bazelbuild/bazel/commit/ef54ef5d17a013c863c4e2fb0583e6bd209645f2.
    instrumented_files_info_has_metadata_files = ge("7.0.0-pre.20230710.5"),
    # Whether treeartifacts can have symlinks pointing outside of the tree artifact. (#21263)
    permits_treeartifact_uplevel_symlinks = ge("7.1.0"),
    # Whether rule extension APIs are available by default
    rule_extension_apis_available = ge("8.0.0rc1"),
)

_toolchains = struct(
    # Whether the mandatory parameter is available on the config_common.toolchain_type function, and thus, whether optional toolchains are supported
    # https://bazel.build/versions/6.0.0/extending/toolchains#optional-toolchains
    has_optional_toolchains = ge("6.0.0"),
)

bazel_features = struct(
    cc = _cc,
    docs = _docs,
    external_deps = _external_deps,
    flags = _flags,
    globals = globals,
    java = _java,
    proto = _proto,
    rules = _rules,
    toolchains = _toolchains,
)
