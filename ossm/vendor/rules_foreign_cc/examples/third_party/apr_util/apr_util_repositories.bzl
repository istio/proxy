"""A module defining the third party dependency apr"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def apr_util_repositories():
    maybe(
        http_archive,
        name = "apr_util",
        build_file = Label("//apr_util:BUILD.apr_util.bazel"),
        sha256 = "b65e40713da57d004123b6319828be7f1273fbc6490e145874ee1177e112c459",
        strip_prefix = "apr-util-1.6.1",
        urls = [
            "https://mirror.bazel.build/www-us.apache.org/dist//apr/apr-util-1.6.1.tar.gz",
            "https://www-us.apache.org/dist//apr/apr-util-1.6.1.tar.gz",
        ],
    )

    maybe(
        http_archive,
        name = "expat",
        build_file = Label("//apr_util:BUILD.expat.bazel"),
        sha256 = "a00ae8a6b96b63a3910ddc1100b1a7ef50dc26dceb65ced18ded31ab392f132b",
        strip_prefix = "expat-2.4.1",
        urls = [
            "https://mirror.bazel.build/github.com/libexpat/libexpat/releases/download/R_2_4_1/expat-2.4.1.tar.gz",
            "https://github.com/libexpat/libexpat/releases/download/R_2_4_1/expat-2.4.1.tar.gz",
        ],
    )
