"""Dependencies needed for the cross-installer tool"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//cargo:defs.bzl", "cargo_bootstrap_repository")

# buildifier: disable=bzl-visibility
load("//rust/private:common.bzl", "DEFAULT_RUST_VERSION")

def cross_installer_deps(**kwargs):
    """Define cross repositories used for compiling cargo-bazel for various platforms.

    Args:
        **kwargs: kwargs to pass through to cargo_bootstrap_repository.

    Returns:
        list[struct(repo=str, is_dev_dep=bool)]: A list of the repositories
        defined by this macro.
    """
    direct_deps = []

    maybe(
        http_archive,
        name = "cross_rs",
        # v0.2.5+
        urls = ["https://github.com/cross-rs/cross/archive/4090beca3cfffa44371a5bba524de3a578aa46c3.zip"],
        strip_prefix = "cross-4090beca3cfffa44371a5bba524de3a578aa46c3",
        integrity = "sha256-9lo/wRsDWdaTzt3kVSBWRfNp+DXeDZqrG3Z+10mE+fo=",
        build_file_content = """exports_files(["Cargo.toml", "Cargo.lock"], visibility = ["//visibility:public"])""",
        patch_args = ["-p1"],
        patches = [
            Label("//crate_universe/tools/cross_installer/patches:cross_rs.static_mut_refs.patch"),
        ],
    )

    direct_deps.append(struct(
        repo = "cross_rs",
        is_dev_dep = True,
    ))

    version = DEFAULT_RUST_VERSION
    if "-" in version:
        version = "nightly/{}".format(version)

    maybe(
        cargo_bootstrap_repository,
        name = "cross_rs_host_bin",
        binary = "cross",
        cargo_toml = "@cross_rs//:Cargo.toml",
        cargo_lockfile = "@cross_rs//:Cargo.lock",
        version = version,
        **kwargs
    )

    direct_deps.append(struct(
        repo = "cross_rs_host_bin",
        is_dev_dep = True,
    ))

    return direct_deps
