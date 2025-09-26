load("@bazel_skylib//rules:build_test.bzl", "build_test")
load("@rules_java//java:defs.bzl", "java_binary", "java_library")
load("//:defs.bzl", "artifact", "java_plugin_artifact")

java_library(
    name = "ClassUsingAnnotationProcessor",
    srcs = ["ClassUsingAnnotationProcessor.java"],
    plugins = [
        java_plugin_artifact("com.google.auto.value:auto-value", "com.google.auto.value.processor.AutoValueProcessor", "service_indexing_testing"),
    ],
    deps = [
        artifact("com.google.auto.value:auto-value-annotations", "service_indexing_testing"),
    ],
)

java_library(
    name = "ClassUsingClassUsingAnnotationProcessor",
    srcs = ["ClassUsingClassUsingAnnotationProcessor.java"],
    deps = [":ClassUsingAnnotationProcessor"],
)

java_binary(
    name = "ClassUsingTestOnlyAnnotationProcessor",
    testonly = True,  # required since plugin dependencies are themselves `testonly`
    srcs = ["ClassUsingTestOnlyAnnotationProcessor.java"],
    main_class = "org.openjdk.jmh.Main",
    plugins = [java_plugin_artifact(
        "org.openjdk.jmh:jmh.generator.annprocess",
        "org.openjdk.jmh.generators.BenchmarkProcessor",
        "service_indexing_testing",
    )],
    deps = [artifact("org.openjdk.jmh:jmh-core", "service_indexing_testing")],
)

build_test(
    name = "build_test",
    targets = [
        ":ClassUsingAnnotationProcessor",
        ":ClassUsingClassUsingAnnotationProcessor",
        ":ClassUsingTestOnlyAnnotationProcessor",
    ],
)
