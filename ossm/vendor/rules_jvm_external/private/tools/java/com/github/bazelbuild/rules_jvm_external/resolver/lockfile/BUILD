load("@rules_java//java:java_library.bzl", "java_library")
load("//private/rules:artifact.bzl", "artifact")

java_library(
    name = "lockfile",
    srcs = glob(["*.java"]),
    visibility = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external:__subpackages__",
        "//tests/com/github/bazelbuild/rules_jvm_external/resolver/lockfile:__pkg__",
    ],
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver",
        artifact(
            "com.google.code.gson:gson",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "com.google.guava:guava",
            repository_name = "rules_jvm_external_deps",
        ),
    ],
)
