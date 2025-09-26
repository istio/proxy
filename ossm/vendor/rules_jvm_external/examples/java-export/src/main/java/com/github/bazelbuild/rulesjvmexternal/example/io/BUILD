load("@rules_jvm_external//:defs.bzl", "artifact", "java_export")

java_export(
    name = "io",
    srcs = glob(["*.java"]),
    maven_coordinates = "com.example:bazel-io:0.0.1",
    visibility = [
        "//:__pkg__",
    ],
    deps = [
        artifact("com.google.guava:guava"),
    ],
)
