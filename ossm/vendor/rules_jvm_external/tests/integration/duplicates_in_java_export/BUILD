load("@protobuf//bazel:java_proto_library.bzl", "java_proto_library")
load("@protobuf//bazel:proto_library.bzl", "proto_library")
load("@rules_java//java:java_library.bzl", "java_library")
load("@rules_java//java:java_test.bzl", "java_test")
load("//:defs.bzl", "artifact", "java_export")

java_test(
    name = "AllowedDuplicatesTest",
    srcs = [
        "AllowedDuplicatesTest.java",
    ],
    data = [
        "child-with-parent-proto",
        "child-without-parent-proto",
    ],
    jvm_flags = [
        "-Dchild.with.parent=$(rootpath :child-with-parent-proto)",
        "-Dchild.without.parent=$(rootpath :child-without-parent-proto)",
    ],
    test_class = "com.github.bazelbuild.rules_jvm_external.duplicates_in_java_export.AllowedDuplicatesTest",
    deps = [
        artifact(
            "junit:junit",
            repository_name = "regression_testing_coursier",
        ),
    ],
)

strip_prefix = "/" + package_name()

proto_library(
    name = "parent_proto",
    srcs = ["parent.proto"],
    strip_import_prefix = strip_prefix,
)

java_proto_library(
    name = "parent_lib",
    deps = [":parent_proto"],
)

java_library(
    name = "parent_resource_jar",
    resource_strip_prefix = package_name(),
    resources = [
        "parent.proto",
    ],
)

java_export(
    name = "parent",
    maven_coordinates = "com.example:parent:1.0.0",
    runtime_deps = [
        ":parent_lib",
        ":parent_resource_jar",
        artifact("com.google.protobuf:protobuf-java"),
    ],
)

proto_library(
    name = "child_proto",
    srcs = ["child.proto"],
    strip_import_prefix = strip_prefix,
    deps = [":parent_proto"],
)

java_proto_library(
    name = "child_lib",
    deps = [":child_proto"],
)

java_export(
    name = "child-without-parent-proto",
    maven_coordinates = "com.example:child:1.0.0",
    resource_strip_prefix = package_name(),
    resources = [
        "child.proto",
    ],
    runtime_deps = [
        ":parent",
        # Simulate an aspect pulling in an extra jar already included in
        # `parent`. This kind of thing happens when using aspects to compile
        # protobufs. By default, we don't want to see `parent.proto` in the
        # constructed maven project jar.
        ":parent_resource_jar",
        ":child_lib",
        artifact("com.google.protobuf:protobuf-java"),
    ],
)

java_export(
    name = "child-with-parent-proto",
    allowed_duplicate_names = [".*\\.proto"],
    maven_coordinates = "com.example:child:1.0.0",
    resource_strip_prefix = package_name(),
    resources = [
        "child.proto",
    ],
    runtime_deps = [
        ":parent",
        # Simulate an aspect pulling in an extra jar already included in
        # `parent`. This kind of thing happens when using aspects to compile
        # protobufs. By default, we don't want to see `parent.proto` in the
        # constructed maven project jar.
        ":parent_resource_jar",
        ":child_lib",
        artifact("com.google.protobuf:protobuf-java"),
    ],
)
