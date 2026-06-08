workspace(name = "proto_field_extraction")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

RULES_PYTHON_TAG = "0.8.1"

RULES_PYTHON_SHA = "cdf6b84084aad8f10bf20b46b77cb48d83c319ebe6458a18e9d2cebf57807cdd"

http_archive(
    name = "rules_python",
    sha256 = RULES_PYTHON_SHA,
    strip_prefix = "rules_python-%s" % RULES_PYTHON_TAG,
    url = "https://github.com/bazelbuild/rules_python/archive/refs/tags/%s.tar.gz" % RULES_PYTHON_TAG,
)

http_archive(
    name = "bazel_skylib",
    sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
        "https://github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
    ],
)

http_archive(
    name = "com_google_googletest",
    sha256 = "730215d76eace9dd49bf74ce044e8daa065d175f1ac891cc1d6bb184ef94e565",
    strip_prefix = "googletest-f53219cdcb7b084ef57414efea92ee5b71989558",
    urls = [
        "https://github.com/google/googletest/archive/f53219cdcb7b084ef57414efea92ee5b71989558.tar.gz",  # 2023-03-16
    ],
)

load("@com_google_googletest//:googletest_deps.bzl", "googletest_deps")

googletest_deps()

# Archive building rules.
http_archive(
    name = "rules_pkg",
    sha256 = "038f1caa773a7e35b3663865ffb003169c6a71dc995e39bf4815792f385d837d",
    urls = [
        "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/0.4.0/rules_pkg-0.4.0.tar.gz",
        "https://github.com/bazelbuild/rules_pkg/releases/download/0.4.0/rules_pkg-0.4.0.tar.gz",
    ],
)

http_archive(
    name = "grpc_httpjson_transcoding",
    strip_prefix = "grpc-httpjson-transcoding-ff41eb3fc9209e6197595b54f7addfa244c0bdb6",  # June 7, 2023
    url = "https://github.com/grpc-ecosystem/grpc-httpjson-transcoding/archive/ff41eb3fc9209e6197595b54f7addfa244c0bdb6.tar.gz",
    #    commit = "ff41eb3fc9209e6197595b54f7addfa244c0bdb6",  # June 7, 2023
    #    remote = "https://github.com/grpc-ecosystem/grpc-httpjson-transcoding.git",
)

# For status_macros
http_archive(
    name = "ocp",
    strip_prefix = "ocp-diag-core-e965ac0ac6db6686169678e2a6c77ede904fa82c/apis/c++",
    url = "https://github.com/opencomputeproject/ocp-diag-core/archive/e965ac0ac6db6686169678e2a6c77ede904fa82c.tar.gz",
)

# -------- Load and call dependencies of underlying libraries --------

load("@grpc_httpjson_transcoding//:repositories.bzl", "absl_repositories", "googleapis_repositories", "io_bazel_rules_docker", "protobuf_repositories", "protoconverter_repositories", "zlib_repositories")

protoconverter_repositories()

googleapis_repositories()

protobuf_repositories()

zlib_repositories()

absl_repositories()

io_bazel_rules_docker()

load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")

switched_rules_by_language(
    name = "com_google_googleapis_imports",
    cc = True,
)

load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")

rules_pkg_dependencies()

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")

bazel_skylib_workspace()

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()
