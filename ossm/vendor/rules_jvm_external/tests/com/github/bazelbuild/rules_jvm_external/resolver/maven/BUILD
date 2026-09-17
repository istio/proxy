load("@rules_java//java:java_test.bzl", "java_test")
load("//:defs.bzl", "artifact")

java_test(
    name = "ConsoleRepositoryListenerTest",
    size = "small",
    srcs = ["ConsoleRepositoryListenerTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.resolver.maven.ConsoleRepositoryListenerTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/maven",
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
        artifact(
            "org.apache.maven.resolver:maven-resolver-api",
            repository_name = "rules_jvm_external_deps",
        ),
    ],
)

java_test(
    name = "DownloaderTest",
    size = "small",
    srcs = ["DownloaderTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.resolver.maven.DownloaderTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/maven",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/netrc",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/remote",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/ui",
        "//tests/com/github/bazelbuild/rules_jvm_external/resolver",
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
    ],
)

java_test(
    name = "MavenResolverTest",
    size = "medium",
    srcs = ["MavenResolverTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.resolver.maven.MavenResolverTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/cmd",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/events",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/maven",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/netrc",
        "//tests/com/github/bazelbuild/rules_jvm_external/resolver",
        artifact(
            "com.google.guava:guava",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
        artifact(
            "org.apache.maven.resolver:maven-resolver-api",
            repository_name = "rules_jvm_external_deps",
        ),
    ],
)
