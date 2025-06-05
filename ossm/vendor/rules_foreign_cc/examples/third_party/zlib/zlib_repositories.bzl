"""A module defining the third party dependency zlib"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def zlib_repositories():
    maybe(
        http_archive,
        name = "zlib",
        build_file = Label("//zlib:BUILD.zlib.bazel"),
        sha256 = "b3a24de97a8fdbc835b9833169501030b8977031bcb54b3b3ac13740f846ab30",
        strip_prefix = "zlib-1.2.13",
        patches = [
            # This patch modifies zlib so that on windows the generated lib matches that stated in the generated pkgconfig pc file for consumption by dependent rules
            # Similar patches are used in vcpkg and conan to resolve the same issue
            Label("//zlib:zlib.patch"),
        ],
        urls = [
            "https://zlib.net/zlib-1.2.13.tar.gz",
            "https://storage.googleapis.com/mirror.tensorflow.org/zlib.net/zlib-1.2.13.tar.gz",
        ],
    )
