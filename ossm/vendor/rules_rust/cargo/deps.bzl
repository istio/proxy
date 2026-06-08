"""
The dependencies for running the cargo_toml_info binary.
"""

load(
    "//cargo/3rdparty/crates:crates.bzl",
    "crate_repositories",
)

def cargo_dependencies():
    """Define dependencies of the `cargo` Bazel tools

    Returns:
        list: A list of all defined repositories.
    """
    direct_deps = []
    direct_deps.extend(crate_repositories())

    return direct_deps
