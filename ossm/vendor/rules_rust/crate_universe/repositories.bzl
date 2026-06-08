"""A module defining dependencies of the `cargo-bazel` Rust target"""

load("@rules_rust//rust:defs.bzl", "rust_common")
load("//cargo/3rdparty/crates:crates.bzl", cargo_crate_repositories = "crate_repositories")
load("//crate_universe:deps_bootstrap.bzl", "cargo_bazel_bootstrap")
load("//crate_universe/3rdparty:third_party_deps.bzl", "third_party_deps")
load("//crate_universe/3rdparty/crates:crates.bzl", _vendor_crate_repositories = "crate_repositories")
load("//crate_universe/private:vendor_utils.bzl", "crates_vendor_deps")

def crate_universe_dependencies(rust_version = rust_common.default_version, bootstrap = False, **kwargs):
    """Define dependencies of the `cargo-bazel` Rust target

    Args:
        rust_version (str, optional): The version of rust to use when generating dependencies.
        bootstrap (bool, optional): If true, a `cargo_bootstrap_repository` target will be generated.
        **kwargs: Arguments to pass through to cargo_bazel_bootstrap.

    Returns:
        list[struct(repo=str, is_dev_dep=bool)]: A list of the repositories
        defined by this macro.
    """
    third_party_deps()

    if bootstrap:
        cargo_bazel_bootstrap(rust_version = rust_version, **kwargs)

    direct_deps = _vendor_crate_repositories()
    direct_deps.extend(crates_vendor_deps())

    # We call this, so that crate_universe users get the deps, but we _don't_ add them to direct_deps.
    # For bzlmod these deps were already added as rules_rust internal deps, and if we add them here we get warnings about duplicates.
    # For WORKSPACE, direct_deps is ignored.
    cargo_crate_repositories()

    return direct_deps
