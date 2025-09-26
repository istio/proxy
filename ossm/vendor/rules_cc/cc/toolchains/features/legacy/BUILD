load("//cc/toolchains:feature_set.bzl", "cc_feature_set")
load("//cc/toolchains/impl:external_feature.bzl", "cc_external_feature")

package(default_visibility = ["//visibility:public"])

# See https://bazel.build/docs/cc-toolchain-config-reference#wellknown-features.

cc_external_feature(
    name = "legacy_compile_flags",
    feature_name = "legacy_compile_flags",
    overridable = True,
)

cc_external_feature(
    name = "default_compile_flags",
    feature_name = "default_compile_flags",
    overridable = True,
)

cc_external_feature(
    name = "dependency_file",
    feature_name = "dependency_file",
    overridable = True,
)

cc_external_feature(
    name = "pic",
    feature_name = "pic",
    overridable = True,
)

cc_external_feature(
    name = "preprocessor_defines",
    feature_name = "preprocessor_defines",
    overridable = True,
)

cc_external_feature(
    name = "includes",
    feature_name = "includes",
    overridable = True,
)

cc_external_feature(
    name = "include_paths",
    feature_name = "include_paths",
    overridable = True,
)

cc_external_feature(
    name = "fdo_instrument",
    feature_name = "fdo_instrument",
    overridable = True,
)

cc_external_feature(
    name = "fdo_optimize",
    feature_name = "fdo_optimize",
    overridable = True,
)

cc_external_feature(
    name = "cs_fdo_instrument",
    feature_name = "cs_fdo_instrument",
    overridable = True,
)

cc_external_feature(
    name = "cs_fdo_optimize",
    feature_name = "cs_fdo_optimize",
    overridable = True,
)

cc_external_feature(
    name = "fdo_prefetch_hints",
    feature_name = "fdo_prefetch_hints",
    overridable = True,
)

cc_external_feature(
    name = "autofdo",
    feature_name = "autofdo",
    overridable = True,
)

cc_external_feature(
    name = "build_interface_libraries",
    feature_name = "build_interface_libraries",
    overridable = True,
)

cc_external_feature(
    name = "dynamic_library_linker_tool",
    feature_name = "dynamic_library_linker_tool",
    overridable = True,
)

cc_external_feature(
    name = "shared_flag",
    feature_name = "shared_flag",
    overridable = True,
)

cc_external_feature(
    name = "linkstamps",
    feature_name = "linkstamps",
    overridable = True,
)

cc_external_feature(
    name = "output_execpath_flags",
    feature_name = "output_execpath_flags",
    overridable = True,
)

cc_external_feature(
    name = "runtime_library_search_directories",
    feature_name = "runtime_library_search_directories",
    overridable = True,
)

cc_external_feature(
    name = "library_search_directories",
    feature_name = "library_search_directories",
    overridable = True,
)

cc_external_feature(
    name = "archiver_flags",
    feature_name = "archiver_flags",
    overridable = True,
)

cc_external_feature(
    name = "libraries_to_link",
    feature_name = "libraries_to_link",
    overridable = True,
)

cc_external_feature(
    name = "force_pic_flags",
    feature_name = "force_pic_flags",
    overridable = True,
)

cc_external_feature(
    name = "user_link_flags",
    feature_name = "user_link_flags",
    overridable = True,
)

cc_external_feature(
    name = "legacy_link_flags",
    feature_name = "legacy_link_flags",
    overridable = True,
)

cc_external_feature(
    name = "static_libgcc",
    feature_name = "static_libgcc",
    overridable = True,
)

cc_external_feature(
    name = "fission_support",
    feature_name = "fission_support",
    overridable = True,
)

cc_external_feature(
    name = "per_object_debug_info",
    feature_name = "per_object_debug_info",
    overridable = True,
)

cc_external_feature(
    name = "strip_debug_symbols",
    feature_name = "strip_debug_symbols",
    overridable = True,
)

cc_external_feature(
    name = "coverage",
    feature_name = "coverage",
    overridable = True,
)

cc_external_feature(
    name = "llvm_coverage_map_format",
    feature_name = "llvm_coverage_map_format",
    overridable = True,
)

cc_external_feature(
    name = "gcc_coverage_map_format",
    feature_name = "gcc_coverage_map_format",
    overridable = True,
)

cc_external_feature(
    name = "fully_static_link",
    feature_name = "fully_static_link",
    overridable = True,
)

cc_external_feature(
    name = "user_compile_flags",
    feature_name = "user_compile_flags",
    overridable = True,
)

cc_external_feature(
    name = "generate_dsym_file",
    feature_name = "generate_dsym_file",
    overridable = True,
)

# Instead of the "sysroot" legacy flag, use the cc_sysroot macro in
# //cc/toolchains/args:sysroot.bzl

cc_external_feature(
    name = "unfiltered_compile_flags",
    feature_name = "unfiltered_compile_flags",
    overridable = True,
)

cc_external_feature(
    name = "linker_param_file",
    feature_name = "linker_param_file",
    overridable = True,
)

cc_external_feature(
    name = "compiler_input_flags",
    feature_name = "compiler_input_flags",
    overridable = True,
)

cc_external_feature(
    name = "compiler_output_flags",
    feature_name = "compiler_output_flags",
    overridable = True,
)

cc_external_feature(
    name = "random_seed",
    feature_name = "random_seed",
    overridable = True,
)

cc_feature_set(
    name = "all_legacy_builtin_features",
    all_of = [
        ":legacy_compile_flags",
        ":default_compile_flags",
        ":dependency_file",
        ":pic",
        ":preprocessor_defines",
        ":includes",
        ":include_paths",
        ":fdo_instrument",
        ":fdo_optimize",
        ":cs_fdo_instrument",
        ":cs_fdo_optimize",
        ":fdo_prefetch_hints",
        ":autofdo",
        ":build_interface_libraries",
        ":dynamic_library_linker_tool",
        ":shared_flag",
        ":linkstamps",
        ":output_execpath_flags",
        ":runtime_library_search_directories",
        ":library_search_directories",
        ":archiver_flags",
        ":libraries_to_link",
        ":force_pic_flags",
        ":user_link_flags",
        ":random_seed",
        ":legacy_link_flags",
        ":static_libgcc",
        ":fission_support",
        ":strip_debug_symbols",
        ":coverage",
        ":llvm_coverage_map_format",
        ":gcc_coverage_map_format",
        ":fully_static_link",
        ":user_compile_flags",
        ":unfiltered_compile_flags",
        ":linker_param_file",
        ":compiler_input_flags",
        ":compiler_output_flags",
    ],
    visibility = ["//cc/toolchains/features:__pkg__"],
)
