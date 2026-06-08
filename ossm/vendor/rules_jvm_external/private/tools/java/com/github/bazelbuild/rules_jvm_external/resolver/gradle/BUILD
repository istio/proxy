load("@rules_java//java:java_binary.bzl", "java_binary")
load("@rules_java//java:java_library.bzl", "java_library")
load("@rules_jvm_external//:defs.bzl", "artifact")

java_library(
    name = "gradle",
    srcs = glob(
        ["*.java"],
        exclude = ["GradleMain.java"],
    ),
    data = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle/data:gradle_build_templates",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle/plugin:plugin-single-jar",
        "@bazel_tools//tools/jdk:host_jdk",
        "@gradle//:distribution",
        "@gradle//:gradle-bin/README",
    ],
    visibility = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver:__subpackages__",
        "//tests/com/github/bazelbuild/rules_jvm_external:__subpackages__",
    ],
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/events",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle/models",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/netrc",
        artifact(
            "org.gradle:gradle-tooling-api",
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
        artifact(
            "com.github.jknack:handlebars",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "com.google.code.gson:gson",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "com.google.guava:guava",
            repository_name = "rules_jvm_external_deps",
        ),
        "@bazel_tools//tools/java/runfiles",
        "@gradle",
    ],
)

java_binary(
    name = "GradleMain",
    srcs = [
        "GradleMain.java",
    ],
    main_class = "com.github.bazelbuild.rules_jvm_external.resolver.gradle.GradleMain",
    resource_strip_prefix = "private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle",
    resources = ["logback.xml"],
    visibility = ["//visibility:public"],
    runtime_deps = [
        artifact(
            "ch.qos.logback:logback-classic",
            repository_name = "rules_jvm_external_deps",
        ),
    ],
    deps = [
        ":gradle",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/cmd",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/events",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/netrc",
    ],
)
