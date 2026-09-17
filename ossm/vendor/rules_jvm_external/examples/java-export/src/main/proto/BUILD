load("@rules_jvm_external//:defs.bzl", "artifact")
load("@rules_proto//proto:defs.bzl", "proto_library")

java_library(
    name = "proto",
    visibility = [
        "//src/main/java:__subpackages__",
    ],
    exports = [
        ":example",
    ],
    runtime_deps = [
        artifact("com.google.protobuf:protobuf-java"),
    ],
)

java_proto_library(
    name = "example",
    deps = [":example_proto"],
)

proto_library(
    name = "example_proto",
    srcs = [
        ":example.proto",
    ],
    deps = [
        "@com_google_protobuf//:duration_proto",
    ],
)
