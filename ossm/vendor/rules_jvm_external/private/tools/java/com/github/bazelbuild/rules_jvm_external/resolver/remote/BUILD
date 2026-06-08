load("@rules_java//java:java_library.bzl", "java_library")
load("@rules_jvm_external//:defs.bzl", "artifact")

java_library(
    name = "remote",
    srcs = glob(["*.java"]),
    visibility = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/maven:__pkg__",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver:__subpackages__",
        "//tests/com/github/bazelbuild/rules_jvm_external/resolver:__subpackages__",
    ],
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/events",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/netrc",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/ui",
        artifact(
            "com.google.guava:guava",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven:maven-model",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.codehaus.plexus:plexus-utils",
            repository_name = "rules_jvm_external_deps",
        ),
    ],
)
