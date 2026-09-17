load("@rules_java//java:java_binary.bzl", "java_binary")
load("@rules_java//java:java_library.bzl", "java_library")
load("@rules_jvm_external//:defs.bzl", "artifact")

java_library(
    name = "maven",
    srcs = glob(
        ["*.java"],
        exclude = ["MavenMain.java"],
    ),
    visibility = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver:__subpackages__",
        "//tests/com/github/bazelbuild/rules_jvm_external:__subpackages__",
    ],
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/events",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/netrc",
        artifact(
            "com.google.guava:guava",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven:maven-artifact",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven:maven-core",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven:maven-model",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven:maven-model-builder",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven:maven-settings",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven:maven-settings-builder",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven.resolver:maven-resolver-api",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven.resolver:maven-resolver-impl",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven:maven-resolver-provider",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven.resolver:maven-resolver-connector-basic",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven.resolver:maven-resolver-spi",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven.resolver:maven-resolver-transport-file",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven.resolver:maven-resolver-transport-http",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.apache.maven.resolver:maven-resolver-util",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.codehaus.plexus:plexus-cipher",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.codehaus.plexus:plexus-sec-dispatcher",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.codehaus.plexus:plexus-utils",
            repository_name = "rules_jvm_external_deps",
        ),
        artifact(
            "org.slf4j:slf4j-simple",
            repository_name = "rules_jvm_external_deps",
        ),
    ],
)

java_binary(
    name = "MavenMain",
    srcs = [
        "MavenMain.java",
    ],
    main_class = "com.github.bazelbuild.rules_jvm_external.resolver.maven.MavenMain",
    visibility = ["//visibility:public"],
    deps = [
        ":maven",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/cmd",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/events",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/resolver/netrc",
    ],
)
