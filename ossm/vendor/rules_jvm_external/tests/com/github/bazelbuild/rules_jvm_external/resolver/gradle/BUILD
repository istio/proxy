load("@rules_java//java:java_test.bzl", "java_test")
load("@rules_jvm_external//:defs.bzl", "artifact")

java_test(
    name = "UnresolvedDepsTest",
    srcs = ["UnresolvedDepsTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.resolver.gradle.UnresolvedDepsTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle/models",
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
    ],
)

java_test(
    name = "GradleResolverTest",
    timeout = "eternal",
    srcs = [
        "GradleModuleMetadataHelper.java",
        "GradleResolverTest.java",
    ],
    data = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle/data:gradle_build_templates",
        "//tests/com/github/bazelbuild/rules_jvm_external/resolver/gradle/fixtures",
    ],
    env = {
        "RJE_VERBOSE": "1",
    },
    test_class = "com.github.bazelbuild.rules_jvm_external.resolver.gradle.GradleResolverTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/cmd",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/events",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/gradle/models",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/netrc",
        "//tests/com/github/bazelbuild/rules_jvm_external/resolver",
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
        artifact(
            "com.google.guava:guava",
            repository_name = "rules_jvm_external_deps",
        ),
        "@bazel_tools//tools/java/runfiles",
    ],
)
