workspace(name = "proto_converter")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

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

http_archive(
    name = "com_google_protobuf",
    sha256 = "aa61db6ff113a1c76eac9408144c6e996c5e2d6b2410818fd7f1b0d222a50bf8",
    strip_prefix = "protobuf-315ffb5be89460f2857387d20aefc59b76b8bdc3", # 5.31.2023
    urls = ["https://github.com/protocolbuffers/protobuf/archive/315ffb5be89460f2857387d20aefc59b76b8bdc3.tar.gz"],
)

http_archive(
    name = "upb",
    patches = ["@com_google_protobuf//build_defs:upb.patch"],
    sha256 = "8c4f3a4fca45da3c7d808a48cb730ae12af6d97462a9ad60e529d29da24cca03",
    strip_prefix = "upb-7a04b4027d737828c9c5b8be56c838d5db0db80f",
    urls = [
        "https://github.com/protocolbuffers/upb/archive/7a04b4027d737828c9c5b8be56c838d5db0db80f.tar.gz",
    ],
)

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

