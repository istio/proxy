"""Dependencies for Rust prost rules"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//private/3rdparty/crates:crates.bzl", "crate_repositories")

def rust_prost_dependencies(bzlmod = False):
    """Declares repositories needed for prost.

    Args:
        bzlmod (bool): Whether bzlmod is enabled.

    Returns:
        list[struct(repo=str, is_dev_dep=bool)]: A list of the repositories
        defined by this macro.
    """

    direct_deps = [
        struct(repo = "rrprd__heck", is_dev_dep = False),
    ]
    if bzlmod:
        # Without bzlmod, this function is normally called by the
        # rust_prost_dependencies function in the private directory.
        # However, the private directory is inaccessible, plus there's no
        # reason to keep two rust_prost_dependencies functions with bzlmod.
        direct_deps.extend(crate_repositories())
    else:
        maybe(
            http_archive,
            name = "rules_proto",
            sha256 = "6fb6767d1bef535310547e03247f7518b03487740c11b6c6adb7952033fe1295",
            strip_prefix = "rules_proto-6.0.2",
            url = "https://github.com/bazelbuild/rules_proto/releases/download/6.0.2/rules_proto-6.0.2.tar.gz",
        )

        maybe(
            http_archive,
            name = "com_google_protobuf",
            integrity = "sha256-fD69eq7dhvpdxHmg/agD9gLKr3jYr/fOg7ieG4rnRCo=",
            strip_prefix = "protobuf-28.3",
            urls = ["https://github.com/protocolbuffers/protobuf/releases/download/v28.3/protobuf-28.3.tar.gz"],
        )
        maybe(
            http_archive,
            name = "zlib",
            build_file = Label("//private/3rdparty:BUILD.zlib.bazel"),
            sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
            strip_prefix = "zlib-1.2.11",
            urls = [
                "https://zlib.net/zlib-1.2.11.tar.gz",
                "https://storage.googleapis.com/mirror.tensorflow.org/zlib.net/zlib-1.2.11.tar.gz",
            ],
        )

    maybe(
        http_archive,
        name = "rrprd__heck",
        integrity = "sha256-IwTgCYP4f/s4tVtES147YKiEtdMMD8p9gv4zRJu+Veo=",
        type = "tar.gz",
        urls = ["https://static.crates.io/crates/heck/heck-0.5.0.crate"],
        strip_prefix = "heck-0.5.0",
        build_file = Label("@rules_rust_prost//private/3rdparty/crates:BUILD.heck-0.5.0.bazel"),
    )
    return direct_deps
