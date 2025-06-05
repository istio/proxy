"""Internal-only utility functions."""

load("@bazel_features_version//:version.bzl", "version")
load(":parse.bzl", "parse_version")

BAZEL_VERSION = parse_version(version)

def le(v):
    return BAZEL_VERSION <= parse_version(v)

def lt(v):
    return BAZEL_VERSION < parse_version(v)

def ne(v):
    return BAZEL_VERSION != parse_version(v)

def ge(v):
    return BAZEL_VERSION >= parse_version(v)

def ge_same_major(v):
    pv = parse_version(v)

    # Version 1.2.3 parses to ([1, 2, 3], ...).
    return BAZEL_VERSION >= pv and BAZEL_VERSION[0][0] == pv[0][0]

def gt(v):
    return BAZEL_VERSION > parse_version(v)
