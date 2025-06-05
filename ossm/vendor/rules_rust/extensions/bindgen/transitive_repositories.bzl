"""Transitive dependencies for the Rust `bindgen` rules"""

load("@llvm-raw//utils/bazel:configure.bzl", "llvm_configure")

# buildifier: disable=unnamed-macro
def rust_bindgen_transitive_dependencies():
    """Declare transitive dependencies needed for bindgen."""

    llvm_configure(
        name = "llvm-project",
        repo_mapping = {
            "@llvm_zlib": "@zlib",
            "@llvm_zstd": "@zstd",
        },
        targets = [
            "AArch64",
            "X86",
        ],
    )
