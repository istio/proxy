load(":compare_feature.bzl", "compare_feature_implementation")

# These tests validate that the produced legacy feature implementations
# properly reflect the implementations housed in the Java source of truth at
# https://github.com/bazelbuild/bazel/blob/master/src/main/java/com/google/devtools/build/lib/rules/cpp/CppActionConfigs.java.
#
# Note: the golden textprotos are not a 1:1 match of the textprotos inlined in
# the Java files. These aim to have identical behavior, but with allowance
# for slight differences in structure due to implementation details. This also
# makes it easier to review the final results.

compare_feature_implementation(
    name = "archiver_flags_test",
    actual_implementation = "//cc/toolchains/args/archiver_flags",
    expected = select({
        "@platforms//os:macos": "//tests/rule_based_toolchain/legacy_features_as_args:goldens/macos/archiver_flags.textproto",
        "//conditions:default": "//tests/rule_based_toolchain/legacy_features_as_args:goldens/unix/archiver_flags.textproto",
    }),
)

compare_feature_implementation(
    name = "force_pic_flags_test",
    actual_implementation = "//cc/toolchains/args/force_pic_flags",
    expected = select({
        "@platforms//os:macos": "//tests/rule_based_toolchain/legacy_features_as_args:goldens/macos/force_pic_flags.textproto",
        "//conditions:default": "//tests/rule_based_toolchain/legacy_features_as_args:goldens/unix/force_pic_flags.textproto",
    }),
)

compare_feature_implementation(
    name = "libraries_to_link_test",
    actual_implementation = "//cc/toolchains/args/libraries_to_link",
    expected = select({
        "@platforms//os:macos": "//tests/rule_based_toolchain/legacy_features_as_args:goldens/macos/libraries_to_link.textproto",
        "//conditions:default": "//tests/rule_based_toolchain/legacy_features_as_args:goldens/unix/libraries_to_link.textproto",
    }),
)

compare_feature_implementation(
    name = "linker_param_file_test",
    actual_implementation = "//cc/toolchains/args/linker_param_file",
    expected = "//tests/rule_based_toolchain/legacy_features_as_args:goldens/unix/linker_param_file.textproto",
)

compare_feature_implementation(
    name = "runtime_library_search_directories_test",
    actual_implementation = "//cc/toolchains/args/runtime_library_search_directories",
    expected = select({
        "@platforms//os:macos": "//tests/rule_based_toolchain/legacy_features_as_args:goldens/macos/runtime_library_search_directories.textproto",
        "//conditions:default": "//tests/rule_based_toolchain/legacy_features_as_args:goldens/unix/runtime_library_search_directories.textproto",
    }),
)

compare_feature_implementation(
    name = "shared_flag_test",
    actual_implementation = "//cc/toolchains/args/shared_flag",
    expected = "//tests/rule_based_toolchain/legacy_features_as_args:goldens/unix/shared_flag.textproto",
)
