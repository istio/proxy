load("@aspect_bazel_lib//lib:diff_test.bzl", "diff_test")
load("@bazel_skylib//rules:build_test.bzl", "build_test")
load("@bazel_skylib//rules:diff_test.bzl", "diff_test")
load("@rules_android//android:rules.bzl", "aar_import")
load("@rules_java//java:java_library.bzl", "java_library")
load("@rules_shell//shell:sh_test.bzl", "sh_test")
load("//tests/integration:is_bzlmod_enabled.bzl", "is_bzlmod_enabled")

aar_import(
    name = "aar_import_that_consumes_the_downloaded_file_directly",
    # Will produce an error if the downloaded file does not have the `.aar` file extension
    aar = "@com_android_support_appcompat_v7_aar_28_0_0//file:file",
    deps = [
        "@starlark_aar_import_test//:com_android_support_animated_vector_drawable",
        "@starlark_aar_import_test//:com_android_support_support_annotations",
        "@starlark_aar_import_test//:com_android_support_support_compat",
        "@starlark_aar_import_test//:com_android_support_support_core_ui",
        "@starlark_aar_import_test//:com_android_support_support_core_utils",
        "@starlark_aar_import_test//:com_android_support_support_fragment",
        "@starlark_aar_import_test//:com_android_support_support_vector_drawable",
    ],
)

build_test(
    name = "test_does_aar_artifact_have_the_correct_file_extension",
    targets = [
        ":aar_import_that_consumes_the_downloaded_file_directly",
    ],
)

java_library(
    name = "additional_deps",
    visibility = ["//visibility:public"],
    exports = [
        "@override_target_in_deps//:original_io_opentelemetry_opentelemetry_api",
    ],
    runtime_deps = [
        "@override_target_in_deps//:redis_clients_jedis",
    ],
)

genquery(
    name = "trace_otel_deps",
    expression = "deps(@override_target_in_deps//:io_opentelemetry_opentelemetry_sdk)",
    opts = [
        "--nohost_deps",
        "--noimplicit_deps",
    ],
    scope = ["@override_target_in_deps//:io_opentelemetry_opentelemetry_sdk"],
)

sh_test(
    name = "override_contains_additional_deps",
    srcs = ["override_contains_additional_deps.sh"],
    data = [
        ":trace_otel_deps",
    ],
    deps = [
        "@bazel_tools//tools/bash/runfiles",
    ],
)

genquery(
    name = "root_module_can_override",
    expression = "deps(@root_module_can_override//:com_squareup_okhttp3_okhttp)",
    opts = [
        "--nohost_deps",
        "--noimplicit_deps",
    ],
    scope = ["@root_module_can_override//:com_squareup_okhttp3_okhttp"],
)

genrule(
    name = "root_module_can_override_sorted",
    testonly = 1,
    srcs = [":root_module_can_override"],
    outs = ["root_module_can_override_sorted.txt"],
    cmd = "cat $< | sed -e 's|^@@|@|g; s|\r||g' | sed -e 's|^@[^/]*[+~]|@|g; s|\r||g' | sort > $@",
)

diff_test(
    name = "root_module_can_override_test",
    file1 = ":root_module_can_override.golden",
    file2 = ":root_module_can_override_sorted.txt",
    # This test only makes sense if we're running with `bzlmod` enabled
    tags = [] if is_bzlmod_enabled() else ["manual"],
)

genquery(
    name = "verify_visibility_query",
    expression = "@root_module_can_override//:com_squareup_okio_okio",
    scope = ["@root_module_can_override//:com_squareup_okio_okio"],
)

sh_test(
    name = "verify_visibility_test",
    srcs = ["verify_visibility.sh"],
    args = ["$(location :verify_visibility_query)"],
    data = [":verify_visibility_query"],
)
