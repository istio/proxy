load("@rules_java//java:java_test.bzl", "java_test")
load("//:defs.bzl", "artifact")

java_test(
    name = "ResourceTest",
    srcs = ["ResourceTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.javadoc.ResourceTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/javadoc:javadoc_lib",
        "//tests/com/github/bazelbuild/rules_jvm_external:zip_utils",
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
        artifact("com.google.guava:guava"),
    ],
)

java_test(
    name = "ExclusionTest",
    srcs = ["ExclusionTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.javadoc.ExclusionTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/javadoc:javadoc_lib",
        "//tests/com/github/bazelbuild/rules_jvm_external:zip_utils",
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
        artifact("com.google.guava:guava"),
    ],
)
