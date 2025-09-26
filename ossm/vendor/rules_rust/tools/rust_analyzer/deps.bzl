"""
The dependencies for running the gen_rust_project binary.
"""

load("//tools/rust_analyzer/3rdparty/crates:defs.bzl", "crate_repositories")

def rust_analyzer_dependencies():
    """Define dependencies of the `rust_analyzer` Bazel tools"""
    return crate_repositories()

# For legacy support
gen_rust_project_dependencies = rust_analyzer_dependencies
rust_analyzer_deps = rust_analyzer_dependencies
