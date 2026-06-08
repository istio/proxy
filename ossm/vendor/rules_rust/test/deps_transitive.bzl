"""Rules rust test dependencies transitive dependencies."""

load("@rules_python//python:repositories.bzl", "py_repositories")

def rules_rust_test_deps_transitive():
    py_repositories()
