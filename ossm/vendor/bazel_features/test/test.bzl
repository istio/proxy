"""Provides a macro to do some loading-time test assertions."""

load("//:features.bzl", "bazel_features")
load("//private:parse.bzl", "parse_version")
load("//private:util.bzl", "BAZEL_VERSION", "ge", "lt")
load("@bazel_skylib//rules:build_test.bzl", "build_test")

def _assert_lt(a, b):
    if parse_version(a) >= parse_version(b):
        fail("expected '{}' to be less than '{}', but was not the case".format(a, b))

def _assert_eq(a, b):
    if parse_version(a) != parse_version(b):
        fail("expected '{}' to be equal to '{}', but was not the case".format(a, b))

def run_test(name):
    """Performs some loading-time assertions about bazel_features' APIs, and creates a fake test target.

    Args:
      name: the name of the fake test target."""

    # some basic version parsing/comparison tests
    _assert_lt("6.1.0", "7.0.0")
    _assert_lt("6.0.1", "6.1.0")
    _assert_lt("6.0.0", "6.0.1")
    _assert_eq("6.0.0rc3", "6.0.0")
    _assert_lt("6.0.0-pre.20241113.4", "6.0.0rc3")
    _assert_lt("6.0.0-pre.20241105.2", "6.0.0-pre.20241113.4")
    _assert_lt("6.0.0 some build metadata", "6.1.0 some other build metadata")
    _assert_lt("6.0.0", "")

    # a smoke test on the actual current Bazel version
    if not ge("0.0.1"):
        fail("somehow the current Bazel version (parsed: '{}') is not >= 0.0.1".format(BAZEL_VERSION))

    if not bazel_features.globals.DefaultInfo == DefaultInfo:
        fail("bazel_features.globals.DefaultInfo != DefaultInfo")

    if lt("8.0.0") != (bazel_features.globals.ProtoInfo != None):
        fail("lt(\"8.0.0\") != (bazel_features.globals.ProtoInfo != None)")

    if not bazel_features.globals.__TestingOnly_NeverAvailable == None:
        fail("bazel_features.globals.__TestingOnly_NeverAvailable != None")

    if (lt("6.0.0-pre.20220630.1") or ge("9.0.0-pre.20250921.2")) and bazel_features.globals.CcSharedLibraryInfo != None:
        fail("bazel_features.globals.CcSharedLibraryInfo != None")
    elif (ge("6.0.0-pre.20220630.1") and lt("9.0.0-pre.20250921.2")) and bazel_features.globals.CcSharedLibraryInfo == None:
        fail("bazel_features.globals.CcSharedLibraryInfo == None")

    # the pseudo test target that doesn't actually test anything
    build_test(
        name = name,
        targets = ["BUILD.bazel"],
    )
