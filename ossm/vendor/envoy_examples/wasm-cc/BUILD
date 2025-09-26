load("@bazel_skylib//lib:selects.bzl", "selects")
load("@envoy//bazel/wasm:wasm.bzl", "envoy_wasm_cc_binary")
load("@rules_pkg//pkg:pkg.bzl", "pkg_tar")

licenses(["notice"])  # Apache 2

exports_files(["example.rst"])

selects.config_setting_group(
    name = "include_wasm_config",
    match_all = [
        "@envoy//bazel:x86",
        "@envoy//bazel:wasm_v8",
    ],
)

filegroup(
    name = "configs",
    srcs = glob([
        "**/*.wasm",
    ]) + select({
        ":include_wasm_config": glob(
            [
                "**/*.yaml",
            ],
            exclude = [
                "**/*docker-compose*.yaml",
            ],
        ),
        "//conditions:default": [],
    }),
    visibility = ["//visibility:public"],
)

envoy_wasm_cc_binary(
    name = "envoy_filter_http_wasm_example.wasm",
    srcs = ["envoy_filter_http_wasm_example.cc"],
)

envoy_wasm_cc_binary(
    name = "envoy_filter_http_wasm_updated_example.wasm",
    srcs = ["envoy_filter_http_wasm_updated_example.cc"],
)

filegroup(
    name = "files",
    srcs = glob(
        ["**/*"],
        exclude = [
            "**/*~",
            "**/.*",
            ".*/**/*",
            "example.rst",
            "BUILD",
            "README.md",
            "WORKSPACE",
        ]),
)

pkg_tar(
    name = "includes",
    srcs = [":files"],
    package_dir = "_include/wasm-cc",
    visibility = ["//visibility:public"],
)
