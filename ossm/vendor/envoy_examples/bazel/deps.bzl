load("@envoy-example-wasmcc//bazel:deps.bzl", "resolve_envoy_example_wasmcc_dependencies")
load("@io_bazel_rules_go//go:deps.bzl", "go_rules_dependencies")
load("@rules_python//python:repositories.bzl", "py_repositories")

def resolve_envoy_examples_dependencies():
    go_rules_dependencies()
    py_repositories()
    resolve_envoy_example_wasmcc_dependencies()
