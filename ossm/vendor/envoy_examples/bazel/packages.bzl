load("@envoy_examples_env//:llvm_flag.bzl", "LLVM_ENABLED")
load("@envoy-example-wasmcc//bazel:packages.bzl", "load_envoy_example_wasmcc_packages")
load("@envoy-example-wasmcc//bazel:toolchains_extra.bzl", "load_envoy_example_wasmcc_toolchains_extra")

def load_envoy_examples_packages():
    load_envoy_example_wasmcc_packages()
    if LLVM_ENABLED:
        load_envoy_example_wasmcc_toolchains_extra()
