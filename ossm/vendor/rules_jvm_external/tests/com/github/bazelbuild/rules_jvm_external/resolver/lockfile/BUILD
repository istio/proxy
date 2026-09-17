load("@rules_java//java:java_test.bzl", "java_test")
load("//:defs.bzl", "artifact")

java_test(
    name = "DependencyIndexTest",
    srcs = ["DependencyIndexTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.resolver.lockfile.DependencyIndexTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/lockfile",
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
        artifact("org.hamcrest:hamcrest"),
        artifact("org.hamcrest:hamcrest_core"),
    ],
)

java_test(
    name = "V3LockFileTest",
    srcs = ["V3LockFileTest.java"],
    resources = ["//:maven_install.json"],
    test_class = "com.github.bazelbuild.rules_jvm_external.resolver.lockfile.V3LockFileTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/cmd",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/lockfile",
        artifact(
            "com.google.code.gson:gson",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "com.google.guava:guava",
            repository_name = "exclusion_testing",
        ),
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
        artifact("org.hamcrest:hamcrest"),
        artifact("org.hamcrest:hamcrest_core"),
    ],
)
