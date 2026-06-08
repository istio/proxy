load("@rules_java//java:java_test.bzl", "java_test")
load("//:defs.bzl", "artifact")

java_test(
    name = "AddJarManifestEntryTest",
    srcs = ["AddJarManifestEntryTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.jar.AddJarManifestEntryTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/jar:AddJarManifestEntry",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/zip",
        artifact("com.google.guava:guava"),
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
    ],
)

java_test(
    name = "IndexJarTest",
    srcs = ["IndexJarTest.java"],
    data = [
        "//tests/custom_maven_install:service_indexing_testing.json",
        "@google_api_services_compute_javadoc_for_test//file",
        "@gson_for_test//file",
        "@hamcrest_core_for_test//file",
        "@hamcrest_core_srcs_for_test//file",
        "@junit_platform_commons_for_test//file",
        "@lombok_for_test//file",
    ],
    test_class = "com.github.bazelbuild.rules_jvm_external.jar.IndexJarTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/jar:IndexJar-lib",
        "@bazel_tools//tools/java/runfiles",
        "@rules_jvm_external_deps//:com_google_code_gson_gson",
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
        artifact("org.hamcrest:hamcrest"),
    ],
)

java_test(
    name = "MergeJarsTest",
    srcs = ["MergeJarsTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.jar.MergeJarsTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/jar:MergeJars",
        "//tests/com/github/bazelbuild/rules_jvm_external:zip_utils",
        artifact("com.google.guava:guava"),
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
    ],
)

java_test(
    name = "CreateJarTest",
    srcs = ["CreateJarTest.java"],
    test_class = "com.github.bazelbuild.rules_jvm_external.jar.CreateJarTest",
    deps = [
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external",
        "//private/tools/java/com/github/bazelbuild/rules_jvm_external/jar:create_jar_lib",
        "//tests/com/github/bazelbuild/rules_jvm_external:zip_utils",
        artifact("com.google.guava:guava"),
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
    ],
)
