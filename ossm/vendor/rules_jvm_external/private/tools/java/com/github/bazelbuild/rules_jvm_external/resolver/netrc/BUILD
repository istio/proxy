load("@rules_java//java:java_library.bzl", "java_library")
load("@rules_jvm_external//:defs.bzl", "artifact")

java_library(
    name = "netrc",
    srcs = glob(["*.java"]),
    visibility = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/maven:__pkg__",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver:__subpackages__",
        "//tests/com/github/bazelbuild/rules_jvm_external:__subpackages__",
    ],
    deps = [
        artifact(
            "com.google.guava:guava",
            repository_name = "rules_jvm_external_deps",
        ),
    ],
)
