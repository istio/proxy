"""Test for py_wheel."""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:truth.bzl", "matching")
load("@rules_testing//lib:util.bzl", rt_util = "util")
load("//python:packaging.bzl", "py_wheel")
load("//tests/base_rules:util.bzl", pt_util = "util")

_tests = []

def _test_too_long_project_url_label(name, config):
    rt_util.helper_target(
        config.rule,
        name = name + "_wheel",
        distribution = name + "_wheel",
        python_tag = "py3",
        version = "0.0.1",
        project_urls = {"This is a label whose length is above the limit!": "www.example.com"},
    )
    analysis_test(
        name = name,
        target = name + "_wheel",
        impl = _test_too_long_project_url_label_impl,
        expect_failure = True,
    )

def _test_too_long_project_url_label_impl(env, target):
    env.expect.that_target(target).failures().contains_predicate(
        matching.str_matches("in `project_urls` is too long"),
    )

_tests.append(_test_too_long_project_url_label)

def py_wheel_test_suite(name):
    config = struct(rule = py_wheel, base_test_rule = py_wheel)
    native.test_suite(
        name = name,
        tests = pt_util.create_tests(_tests, config = config),
    )
