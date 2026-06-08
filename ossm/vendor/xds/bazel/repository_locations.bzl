REPOSITORY_LOCATIONS = dict(
    bazel_gazelle = dict(
        sha256 = "e467b801046b6598c657309b45d2426dc03513777bd1092af2c62eebf990aca5",
        urls = [
            "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.45.0/bazel-gazelle-v0.45.0.tar.gz",
        ],
    ),
    bazel_skylib = dict(
        sha256 = "6e78f0e57de26801f6f564fa7c4a48dc8b36873e416257a92bbb0937eeac8446",
        urls = ["https://github.com/bazelbuild/bazel-skylib/releases/download/1.8.2/bazel-skylib-1.8.2.tar.gz"],
    ),
    com_envoyproxy_protoc_gen_validate = dict(
        sha256 = "e4718352754df1393b8792b631338aa8562f390e8160783e365454bc11d96328",
        strip_prefix = "protoc-gen-validate-1.2.1",
        urls = ["https://github.com/envoyproxy/protoc-gen-validate/archive/refs/tags/v1.2.1.tar.gz"],
    ),
    com_github_grpc_grpc = dict(
        sha256 = "86f0f28b75b1562c82b7fc5d436b4f5f5da6cb8d67f59e9fec5f8aa1fe924037",
        strip_prefix = "grpc-1.68.0",
        urls = ["https://github.com/grpc/grpc/archive/refs/tags/v1.68.0.tar.gz"],
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
        sha256 = "008a11cc56f9b96679b4c285fd05f46d317d685be3ab524b2a310be0fbad987e",
        strip_prefix = "protobuf-29.3",
        urls = ["https://github.com/protocolbuffers/protobuf/archive/v29.3.tar.gz"],
    ),
    dev_cel = dict(
        sha256 = "5cba6b0029e727d1f4d8fd134de4e747cecc0bc293d026017d7edc48058d09f7",
        strip_prefix = "cel-spec-0.24.0",
        urls = ["https://github.com/google/cel-spec/archive/refs/tags/v0.24.0.tar.gz"],
    ),
    io_bazel_rules_go = dict(
        sha256 = "b78f77458e77162f45b4564d6b20b6f92f56431ed59eaaab09e7819d1d850313",
        urls = [
            "https://github.com/bazelbuild/rules_go/releases/download/v0.53.0/rules_go-v0.53.0.zip",
        ],
    ),
    rules_proto = dict(
        sha256 = "14a225870ab4e91869652cfd69ef2028277fc1dc4910d65d353b62d6e0ae21f4",
        strip_prefix = "rules_proto-7.1.0",
        urls = ["https://github.com/bazelbuild/rules_proto/archive/refs/tags/7.1.0.tar.gz"],
    ),
    rules_python = dict(
        sha256 = "2f5c284fbb4e86045c2632d3573fc006facbca5d1fa02976e89dc0cd5488b590",
        strip_prefix = "rules_python-1.6.3",
        urls = ["https://github.com/bazelbuild/rules_python/archive/1.6.3.tar.gz"],
    ),
)
