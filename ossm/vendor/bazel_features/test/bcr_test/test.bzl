"""Tests for `bazel_features` module."""

load("@bazel_features//:features.bzl", "bazel_features")
load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")

def _is_bzlmod_enabled_test(ctx):
    env = unittest.begin(ctx)

    asserts.true(env, bazel_features.external_deps.is_bzlmod_enabled)

    return unittest.end(env)

is_bzlmod_enabled_test = unittest.make(_is_bzlmod_enabled_test)

def bazel_features_test_suite(name):
    return unittest.suite(
        name,
        is_bzlmod_enabled_test,
    )
