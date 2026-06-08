"""Transitive repositories for Rust dependencies."""

load("@bazel_features//:deps.bzl", "bazel_features_deps")
load("@rules_cc//cc:repositories.bzl", "rules_cc_dependencies")

def rules_rust_transitive_dependencies():
    """Rust transitive repositories."""
    rules_cc_dependencies()
    bazel_features_deps()
