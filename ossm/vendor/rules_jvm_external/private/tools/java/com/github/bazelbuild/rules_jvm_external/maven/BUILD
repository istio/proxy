load("@rules_java//java:java_binary.bzl", "java_binary")
load("@rules_java//java:java_library.bzl", "java_library")
load("@rules_jvm_external//:defs.bzl", "artifact")

java_binary(
    name = "MavenPublisher",
    main_class = "com.github.bazelbuild.rules_jvm_external.maven.MavenPublisher",
    visibility = ["//visibility:public"],
    runtime_deps = [
        ":maven-publisher-lib",
    ],
)

java_library(
    name = "maven-publisher-lib",
    srcs = [
        "MavenPublisher.java",
        "MavenSigning.java",
    ],
    visibility = ["//tests/com/github/bazelbuild/rules_jvm_external/maven:__pkg__"],
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/netrc",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/remote",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/ui",
        artifact(
            "com.google.code.findbugs:jsr305",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "software.amazon.awssdk:sdk-core",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "com.google.auth:google-auth-library-oauth2-http",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "com.google.auth:google-auth-library-credentials",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "com.google.cloud:google-cloud-storage",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "com.google.cloud:google-cloud-core",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "com.google.guava:guava",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "software.amazon.awssdk:s3",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.bouncycastle:bcprov-jdk15on",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.bouncycastle:bcpg-jdk15on",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven:maven-repository-metadata",
            repository_name = "rules_jvm_external_deps",
        ),
    ],
)

java_binary(
    name = "outdated",
    srcs = ["Outdated.java"],
    main_class = "com.github.bazelbuild.rules_jvm_external.maven.Outdated",
    visibility = ["//visibility:public"],
    deps = [
        artifact(
            "org.apache.maven:maven-artifact",
            repository_name = "rules_jvm_external_deps",
        ),
    ],
)
