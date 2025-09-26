load("//:defs.bzl", "artifact")

java_test(
    name = "OutdatedTest",
    srcs = ["OutdatedTest.java"],
    data = [
        "//tests/com/github/bazelbuild/rules_jvm_external/maven/resources",
    ],
    resources = [
        "//tests/com/github/bazelbuild/rules_jvm_external/maven/resources",
    ],
    test_class = "com.github.bazelbuild.rules_jvm_external.maven.OutdatedTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/maven:outdated",
        artifact("com.google.guava:guava"),
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
        artifact(
            "org.apache.maven:maven-artifact",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact("org.hamcrest:hamcrest"),
    ],
)
