licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "authn_wasm_lib",
    srcs = [
        "config.cc",
    ],
    hdrs = [
        "config.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//external:authentication_policy_config_cc_proto",
        "@envoy//source/extensions/common/wasm/null:null_lib",
    ],
)
