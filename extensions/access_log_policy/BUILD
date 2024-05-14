load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

envoy_cc_library(
    name = "istio_dimensions",
    hdrs = [
        "istio_dimensions.h",
    ],
    repository = "@envoy",
)

envoy_cc_test(
    name = "istio_dimensions_test",
    size = "small",
    srcs = ["istio_dimensions_test.cc"],
    external_deps = ["abseil_hash_testing"],
    repository = "@envoy",
    deps = [
        ":istio_dimensions",
    ],
)

envoy_cc_library(
    name = "access_log_policy_lib",
    srcs = [
        "config.cc",
        "plugin.cc",
    ],
    hdrs = [
        "plugin.h",
    ],
    repository = "@envoy",
    deps = [
        ":istio_dimensions",
        "//extensions/access_log_policy/config/v1alpha1:access_log_policy_config_cc_proto",
        "//extensions/stackdriver:context",
        "@envoy//source/common/common:base64_lib",
        "@proxy_wasm_cpp_host//:null_lib",
    ],
)
