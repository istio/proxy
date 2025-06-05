load("@rules_java//java:defs.bzl", "java_binary")
load("@rules_jvm_external//:defs.bzl", "artifact")

java_binary(
    name = "MavenPublisher",
    srcs = [
        "MavenPublisher.java",
        "MavenSigning.java",
    ],
    main_class = "com.github.bazelbuild.rules_jvm_external.maven.MavenPublisher",
    visibility = ["//visibility:public"],
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
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
