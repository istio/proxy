"""A module is used to assist in bootstrapping cargo-bazel"""

load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//cargo:defs.bzl", "cargo_bootstrap_repository")
load("//crate_universe/private:srcs.bzl", "CARGO_BAZEL_SRCS")

# buildifier: disable=bzl-visibility
load("//rust/private:common.bzl", "rust_common")

def cargo_bazel_bootstrap(
        name = "cargo_bazel_bootstrap",
        rust_version = rust_common.default_version,
        **kwargs):
    """An optional repository which bootstraps `cargo-bazel` for use with `crates_repository`

    Args:
        name (str, optional): The name of the `cargo_bootstrap_repository`.
        rust_version (str, optional): The rust version to use. Defaults to the default of `cargo_bootstrap_repository`.
        **kwargs: kwargs to pass through to cargo_bootstrap_repository.

    Returns:
        list[struct(repo=str, is_dev_dep=bool)]: A list of the repositories
        defined by this macro.
    """

    maybe(
        cargo_bootstrap_repository,
        name = name,
        srcs = CARGO_BAZEL_SRCS,
        binary = "cargo-bazel",
        cargo_lockfile = "@rules_rust//crate_universe:Cargo.lock",
        cargo_toml = "@rules_rust//crate_universe:Cargo.toml",
        version = rust_version,
        # The increased timeout helps avoid flakes in CI
        timeout = 900,
        **kwargs
    )

    return [struct(
        repo = name,
        is_dev_dep = False,
    )]
