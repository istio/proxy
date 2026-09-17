load("@rules_java//java:java_library.bzl", "java_library")
load("@rules_java//java:java_single_jar.bzl", "java_single_jar")
load("@rules_jvm_external//:defs.bzl", "artifact")

java_library(
    name = "plugin",
    srcs = glob(["*.java"]),
    visibility = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle:__subpackages__",
        "//tests:__subpackages__",
    ],
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle/models",
        "@gradle",
        artifact(
            "org.gradle:gradle-tooling-api",
            repository_name = "rules_jvm_external_deps",
        ),
    ],
)

java_single_jar(
    name = "plugin-single-jar",
    visibility = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle:__subpackages__",
        "//tests:__subpackages__",
    ],
    deps = [
        ":plugin",
    ],
)
