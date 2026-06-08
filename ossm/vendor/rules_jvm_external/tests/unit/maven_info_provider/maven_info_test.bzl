"""Unit tests for java_export MavenInfo behavior."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("@rules_java//java:java_library.bzl", "java_library")
load("//private/rules:has_maven_deps.bzl", "MavenInfo", "has_maven_deps")

def _maven_info_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)

    asserts.equals(env, None, tut[MavenInfo].coordinates)

    # target under test has no coordinates, so `as_maven_dep` should be equal to its maven_deps
    asserts.equals(
        env,
        sorted(tut[MavenInfo].maven_deps.to_list()),
        sorted(tut[MavenInfo].as_maven_dep.to_list()),
    )

    asserts.equals(env, [
        # keep sorted
        "example:dep_leaf:1.0.0",
        "example:exported_leaf:1.0.0",
        "example:leaf:1.0.0",
        "example:runtime_dep_and_dep_leaf:1.0.0",
        "example:runtime_dep_leaf:1.0.0",
    ], sorted(tut[MavenInfo].maven_deps.to_list()))

    asserts.equals(env, [
        # keep sorted
        "example:exported_leaf:1.0.0",
    ], sorted(tut[MavenInfo].maven_export_deps.to_list()))

    return analysistest.end(env)

_maven_info_test = analysistest.make(
    _maven_info_test_impl,
    extra_target_under_test_aspects = [has_maven_deps],
)

def maven_info_tests(name):
    java_library(
        name = "library_to_test",
        deps = [
            ":is_dep_has_exports",
            ":is_dep_has_runtime_deps",
        ],
        exports = [":is_export_has_deps"],
        runtime_deps = [":is_runtime_dep_has_deps"],
        srcs = ["Foo.java"],
    )

    java_library(
        name = "is_dep_has_exports",
        exports = [
            ":leaf",
            ":runtime_dep_and_dep_leaf",
            ":stop_propagation",
        ],
        srcs = ["Foo.java"],
    )

    java_library(
        name = "is_dep_has_runtime_deps",
        runtime_deps = [
            ":runtime_dep_leaf",
            ":runtime_dep_and_dep_leaf",
        ],
        srcs = ["Foo.java"],
    )

    java_library(
        name = "is_export_has_deps",
        deps = [":exported_leaf"],
        srcs = ["Foo.java"],
    )

    java_library(
        name = "is_runtime_dep_has_deps",
        deps = [":dep_leaf"],
        srcs = ["Foo.java"],
    )

    java_library(
        name = "leaf",
        srcs = ["Foo.java"],
        tags = [
            "maven_coordinates=example:leaf:1.0.0",
        ],
    )

    java_library(
        name = "exported_leaf",
        srcs = ["Foo.java"],
        tags = [
            "maven_coordinates=example:exported_leaf:1.0.0",
        ],
    )

    java_library(
        name = "dep_leaf",
        srcs = ["Foo.java"],
        tags = [
            "maven_coordinates=example:dep_leaf:1.0.0",
        ],
    )

    java_library(
        name = "runtime_dep_leaf",
        srcs = ["Foo.java"],
        tags = [
            "maven_coordinates=example:runtime_dep_leaf:1.0.0",
        ],
    )

    java_library(
        name = "runtime_dep_and_dep_leaf",
        srcs = ["Foo.java"],
        tags = [
            "maven_coordinates=example:runtime_dep_and_dep_leaf:1.0.0",
        ],
        deps = [":transitive_maven_dep"],
    )

    java_library(
        name = "stop_propagation",
        srcs = ["Foo.java"],
        tags = [
            "no-maven",
        ],
        deps = [":not_included_leaf"],
    )

    java_library(
        name = "not_included_leaf",
        srcs = ["Foo.java"],
        tags = [
            "maven_coordinates=example:not_included_leaf:1.0.0",
        ],
    )

    java_library(
        name = "transitive_maven_dep",
        srcs = ["Foo.java"],
        tags = [
            "maven_coordinates=example:transitive_maven_dep:1.0.0",
        ],
    )

    _maven_info_test(
        name = name,
        target_under_test = ":library_to_test",
    )
