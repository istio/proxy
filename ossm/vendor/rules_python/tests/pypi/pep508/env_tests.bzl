"""Tests to check for env construction."""

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python/private/pypi:pep508_env.bzl", pep508_env = "env")  # buildifier: disable=bzl-visibility

_tests = []

def _test_env_defaults(env):
    got = pep508_env(os = "exotic", arch = "exotic", python_version = "3.1.1")
    got.pop("_aliases")
    env.expect.that_dict(got).contains_exactly({
        "implementation_name": "cpython",
        "implementation_version": "3.1.1",
        "os_name": "posix",
        "platform_machine": "",
        "platform_python_implementation": "CPython",
        "platform_release": "",
        "platform_system": "",
        "platform_version": "0",
        "python_full_version": "3.1.1",
        "python_version": "3.1",
        "sys_platform": "",
    })

_tests.append(_test_env_defaults)

def _test_env_freebsd(env):
    got = pep508_env(os = "freebsd", arch = "arm64", python_version = "3.1.1")
    got.pop("_aliases")
    env.expect.that_dict(got).contains_exactly({
        "implementation_name": "cpython",
        "implementation_version": "3.1.1",
        "os_name": "posix",
        "platform_machine": "aarch64",
        "platform_python_implementation": "CPython",
        "platform_release": "",
        "platform_system": "FreeBSD",
        "platform_version": "0",
        "python_full_version": "3.1.1",
        "python_version": "3.1",
        "sys_platform": "freebsd",
    })

_tests.append(_test_env_freebsd)

def _test_env_macos(env):
    got = pep508_env(os = "macos", arch = "arm64", python_version = "3.1.1")
    got.pop("_aliases")
    env.expect.that_dict(got).contains_exactly({
        "implementation_name": "cpython",
        "implementation_version": "3.1.1",
        "os_name": "posix",
        "platform_machine": "aarch64",
        "platform_python_implementation": "CPython",
        "platform_release": "",
        "platform_system": "Darwin",
        "platform_version": "0",
        "python_full_version": "3.1.1",
        "python_version": "3.1",
        "sys_platform": "darwin",
    })

_tests.append(_test_env_macos)

def env_test_suite(name):  # buildifier: disable=function-docstring
    test_suite(
        name = name,
        basic_tests = _tests,
    )
