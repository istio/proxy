load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def cpp2sky_dependencies():
    rules_proto()
    skywalking_data_collect_protocol()
    com_github_grpc_grpc()
    com_google_googletest()
    com_google_protobuf()
    com_github_httplib()
    com_github_fmtlib_fmt()
    com_google_abseil()
    com_github_gabime_spdlog()
    hedron_compile_commands()

def skywalking_data_collect_protocol():
    http_archive(
        name = "skywalking_data_collect_protocol",
        sha256 = "4cf7cf84a9478a09429a7fbc6ad1e1b10c70eb54999438a36eacaf539a39d343",
        urls = [
            "https://github.com/apache/skywalking-data-collect-protocol/archive/v9.1.0.tar.gz",
        ],
        strip_prefix = "skywalking-data-collect-protocol-9.1.0",
    )

def com_github_grpc_grpc():
    http_archive(
        name = "com_github_grpc_grpc",
        sha256 = "1ccc2056b68b81ada8df61310e03dfa0541c34821fd711654d0590a7321db9c8",
        urls = ["https://github.com/grpc/grpc/archive/a3ae8e00a2c5553c806e83fae83e33f0198913f0.tar.gz"],
        strip_prefix = "grpc-a3ae8e00a2c5553c806e83fae83e33f0198913f0",
    )

def rules_proto():
    http_archive(
        name = "rules_proto",
        sha256 = "66bfdf8782796239d3875d37e7de19b1d94301e8972b3cbd2446b332429b4df1",
        strip_prefix = "rules_proto-4.0.0",
        urls = [
            "https://github.com/bazelbuild/rules_proto/archive/refs/tags/4.0.0.tar.gz",
        ],
    )

def com_google_googletest():
    http_archive(
        name = "com_google_googletest",
        sha256 = "7897bfaa5ad39a479177cfb5c3ce010184dbaee22a7c3727b212282871918751",
        strip_prefix = "googletest-a4ab0abb93620ce26efad9de9296b73b16e88588",
        urls = ["https://github.com/google/googletest/archive/a4ab0abb93620ce26efad9de9296b73b16e88588.tar.gz"],
    )

def com_google_protobuf():
    http_archive(
        name = "com_google_protobuf",
        sha256 = "89ac31a93832e204db6d73b1e80f39f142d5747b290f17340adce5be5b122f94",
        strip_prefix = "protobuf-3.19.4",
        urls = ["https://github.com/protocolbuffers/protobuf/releases/download/v3.19.4/protobuf-cpp-3.19.4.tar.gz"],
    )

def com_github_httplib():
    http_archive(
        name = "com_github_httplib",
        sha256 = "0e424f92b607fc9245c144dada85c2e97bc6cc5938c0c69a598a5b2a5c1ab98a",
        strip_prefix = "cpp-httplib-0.7.15",
        build_file = "//bazel:httplib.BUILD",
        urls = ["https://github.com/yhirose/cpp-httplib/archive/v0.7.15.tar.gz"],
    )

def com_github_fmtlib_fmt():
    http_archive(
        name = "com_github_fmtlib_fmt",
        sha256 = "23778bad8edba12d76e4075da06db591f3b0e3c6c04928ced4a7282ca3400e5d",
        strip_prefix = "fmt-8.1.1",
        build_file = "//bazel:fmtlib.BUILD",
        urls = ["https://github.com/fmtlib/fmt/releases/download/8.1.1/fmt-8.1.1.zip"],
    )

def com_github_gabime_spdlog():
    http_archive(
        name = "com_github_gabime_spdlog",
        sha256 = "6fff9215f5cb81760be4cc16d033526d1080427d236e86d70bb02994f85e3d38",
        strip_prefix = "spdlog-1.9.2",
        build_file = "//bazel:spdlog.BUILD",
        urls = ["https://github.com/gabime/spdlog/archive/v1.9.2.tar.gz"],
    )

def com_google_abseil():
    http_archive(
        name = "com_google_absl",
        sha256 = "5ca73792af71ab962ee81cdf575f79480704b8fb87e16ca8f1dc1e9b6822611e",
        strip_prefix = "abseil-cpp-6f43f5bb398b6685575b36874e36cf1695734df1",
        urls = ["https://github.com/abseil/abseil-cpp/archive/6f43f5bb398b6685575b36874e36cf1695734df1.tar.gz"],
    )

def hedron_compile_commands():
    # Hedron's Compile Commands Extractor for Bazel
    # https://github.com/hedronvision/bazel-compile-commands-extractor
    http_archive(
        name = "hedron_compile_commands",
        url = "https://github.com/hedronvision/bazel-compile-commands-extractor/archive/dc36e462a2468bd79843fe5176542883b8ce4abe.tar.gz",
        sha256 = "d63c1573eb1daa4580155a1f0445992878f4aa8c34eb165936b69504a8407662",
        strip_prefix = "bazel-compile-commands-extractor-dc36e462a2468bd79843fe5176542883b8ce4abe",
    )
