licenses(["notice"])  # Apache 2

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_package",
)

envoy_package()

filegroup(
    name = "testdata",
    srcs = glob([
        "*.json",
        "*.wasm",
        "*.yaml",
    ]),
)
