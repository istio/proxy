load("@rules_java//java:java_library.bzl", "java_library")
load("@rules_java//java:java_test.bzl", "java_test")
load("//:defs.bzl", "artifact")

java_test(
    name = "ArtifactExclusionsTest",
    srcs = ["ArtifactExclusionsTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.ArtifactExclusionsTest",
    deps = [
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

java_test(
    name = "CoordinatesTest",
    srcs = ["CoordinatesTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.CoordinatesTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
    ],
)

java_test(
    name = "GlobalArtifactExclusionsTest",
    srcs = ["GlobalArtifactExclusionsTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.GlobalArtifactExclusionsTest",
    deps = [
        artifact(
            "com.diffplug.durian:durian-core",
            repository_name = "global_exclusion_testing",
        ),
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
        artifact(
            "com.google.guava:guava",
            repository_name = "global_exclusion_testing",
        ),
        artifact(
            "com.squareup.okhttp3:okhttp",
            repository_name = "global_exclusion_testing",
        ),
        artifact("org.hamcrest:hamcrest"),
        artifact("org.hamcrest:hamcrest-core"),
    ],
)

java_test(
    name = "HasherTest",
    size = "small",
    srcs = ["HasherTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.HasherTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external:hasher",
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
        artifact("org.hamcrest:hamcrest"),
    ],
)

java_test(
    name = "JsonArtifactsTest",
    srcs = ["JsonArtifactsTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.JsonArtifactsTest",
    deps = [
        artifact(
            "io.quarkus:quarkus-maven-plugin",
            repository_name = "json_artifacts_testing",
        ),
        artifact(
            "org.json:json",
            repository_name = "json_artifacts_testing",
        ),
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
    ],
)

java_test(
    name = "NeverlinkTest",
    srcs = ["NeverlinkTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.NeverlinkTest",
    deps = [
        artifact("com.google.guava:guava"),
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
        artifact("org.hamcrest:hamcrest"),
    ],
)

java_library(
    name = "zip_utils",
    testonly = 1,
    srcs = ["ZipUtils.java"],
    visibility = ["//:__subpackages__"],
    deps = [
        artifact("com.google.guava:guava"),
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/zip",
    ],
)
