"""Analysis test for for rust_bindgen_library rule."""

load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_rust//rust:defs.bzl", "rust_binary")
load("@rules_rust_bindgen//:defs.bzl", "rust_bindgen_library")
load("@rules_testing//lib:analysis_test.bzl", "analysis_test", "test_suite")

def _test_cc_linkopt_impl(env, target):
    # Assert
    env.expect.that_action(target.actions[0]) \
        .contains_at_least_args(["--codegen=link-arg=-shared"])

def _test_cc_linkopt(name):
    # Arrange
    cc_library(
        name = name + "_cc",
        srcs = ["simple.cc"],
        hdrs = ["simple.h"],
        linkopts = ["-shared"],
        tags = ["manual"],
    )
    rust_bindgen_library(
        name = name + "_rust_bindgen",
        cc_lib = name + "_cc",
        header = "simple.h",
        tags = ["manual"],
        edition = "2021",
    )
    rust_binary(
        name = name + "_rust_binary",
        srcs = ["main.rs"],
        deps = [name + "_rust_bindgen"],
        tags = ["manual"],
        edition = "2021",
    )

    # Act
    # TODO: Use targets attr to also verify `rust_bindgen_library` not having
    # the linkopt after https://github.com/bazelbuild/rules_testing/issues/67
    # is released
    analysis_test(
        name = name,
        target = name + "_rust_binary",
        impl = _test_cc_linkopt_impl,
    )

def _test_cc_lib_object_merging_impl(env, target):
    env.expect.that_int(len(target.actions)).is_greater_than(2)
    env.expect.that_action(target.actions[0]).mnemonic().contains("RustBindgen")
    env.expect.that_action(target.actions[1]).mnemonic().contains("FileWrite")
    env.expect.that_action(target.actions[1]).content().contains("-lstatic=test_cc_lib_object_merging_cc")
    env.expect.that_action(target.actions[2]).mnemonic().contains("FileWrite")
    env.expect.that_action(target.actions[2]).content().contains("-Lnative=")

def _test_cc_lib_object_merging_disabled_impl(env, target):
    # no FileWrite actions writing arg files registered
    env.expect.that_int(len(target.actions)).is_greater_than(0)
    env.expect.that_action(target.actions[0]).mnemonic().contains("RustBindgen")

def _test_cc_lib_object_merging(name):
    cc_library(
        name = name + "_cc",
        hdrs = ["simple.h"],
        srcs = ["simple.cc"],
    )

    rust_bindgen_library(
        name = name + "_rust_bindgen",
        cc_lib = name + "_cc",
        header = "simple.h",
        tags = ["manual"],
        edition = "2021",
    )

    analysis_test(
        name = name,
        target = name + "_rust_bindgen__bindgen",
        impl = _test_cc_lib_object_merging_impl,
    )

def _test_cc_lib_object_merging_disabled(name):
    cc_library(
        name = name + "_cc",
        hdrs = ["simple.h"],
        srcs = ["simple.cc"],
    )

    rust_bindgen_library(
        name = name + "_rust_bindgen",
        cc_lib = name + "_cc",
        header = "simple.h",
        tags = ["manual"],
        merge_cc_lib_objects_into_rlib = False,
        edition = "2021",
    )

    analysis_test(
        name = name,
        target = name + "_rust_bindgen__bindgen",
        impl = _test_cc_lib_object_merging_disabled_impl,
    )

def bindgen_test_suite(name):
    test_suite(
        name = name,
        tests = [
            _test_cc_linkopt,
            _test_cc_lib_object_merging,
            _test_cc_lib_object_merging_disabled,
        ],
    )
