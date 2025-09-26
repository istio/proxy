REPOSITORY_LOCATIONS = dict(
    bazel_gazelle = dict(
        sha256 = "b7387f72efb59f876e4daae42f1d3912d0d45563eac7cb23d1de0b094ab588cf",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.34.0/bazel-gazelle-v0.34.0.tar.gz",
            "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.34.0/bazel-gazelle-v0.34.0.tar.gz",
        ],
    ),
    bazel_skylib = dict(
        sha256 = "1c531376ac7e5a180e0237938a2536de0c54d93f5c278634818e0efc952dd56c",
        urls = ["https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.0.3/bazel-skylib-1.0.3.tar.gz"],
    ),
    com_envoyproxy_protoc_gen_validate = dict(
        sha256 = "92e29c2150675ce954c965bcaa559ca944704b75711533cfe03ce541dcf5a1dd",
        strip_prefix = "protoc-gen-validate-1.0.4",
        urls = ["https://github.com/envoyproxy/protoc-gen-validate/archive/refs/tags/v1.0.4.tar.gz"],
    ),
    com_github_grpc_grpc = dict(
        sha256 = "916f88a34f06b56432611aaa8c55befee96d0a7b7d7457733b9deeacbc016f99",
        strip_prefix = "grpc-1.59.1",
        urls = ["https://github.com/grpc/grpc/archive/refs/tags/v1.59.1.tar.gz"],
    ),
    com_google_googleapis = dict(
        # TODO(dio): Consider writing a Starlark macro for importing Google API proto.
        sha256 = "9d1a930e767c93c825398b8f8692eca3fe353b9aaadedfbcf1fca2282c85df88",
        strip_prefix = "googleapis-64926d52febbf298cb82a8f472ade4a3969ba922",
        urls = [
            "https://github.com/googleapis/googleapis/archive/64926d52febbf298cb82a8f472ade4a3969ba922.zip",
        ],
    ),
    com_google_protobuf = dict(
        sha256 = "8242327e5df8c80ba49e4165250b8f79a76bd11765facefaaecfca7747dc8da2",
        strip_prefix = "protobuf-3.21.5",
        urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.21.5.zip"],
    ),
    dev_cel = dict(
        sha256 = "3ee09eb69dbe77722e9dee23dc48dc2cd9f765869fcf5ffb1226587c81791a0b",
        strip_prefix = "cel-spec-0.15.0",
        urls = ["https://github.com/google/cel-spec/archive/refs/tags/v0.15.0.tar.gz"],
    ),
    io_bazel_rules_go = dict(
        sha256 = "80a98277ad1311dacd837f9b16db62887702e9f1d1c4c9f796d0121a46c8e184",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.46.0/rules_go-v0.46.0.zip",
            "https://github.com/bazelbuild/rules_go/releases/download/v0.46.0/rules_go-v0.46.0.zip",
        ],
    ),
    rules_proto = dict(
        sha256 = "80d3a4ec17354cccc898bfe32118edd934f851b03029d63ef3fc7c8663a7415c",
        strip_prefix = "rules_proto-5.3.0-21.5",
        urls = ["https://github.com/bazelbuild/rules_proto/archive/refs/tags/5.3.0-21.5.tar.gz"],
    ),
)
