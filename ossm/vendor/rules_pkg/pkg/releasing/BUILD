load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:public"],
)

# WARNING: 2021-06-28. This is experimental and subject to change.

# Sample usage:
# load("@rules_pkg//releasing:defs.bzl", "print_rel_notes")
# print_rel_notes(
#     name = "relnotes",
#     repo = "rules_pkg",
#     version = "2.1",
#     outs = ["relnotes.txt"],
# )

filegroup(
    name = "standard_package",
    srcs = ["BUILD"] + glob([
        "*.bzl",
        "*.py",
    ]),
    visibility = [
        "//distro:__pkg__",
        "//pkg:__pkg__",
    ],
)

py_library(
    name = "release_utils",
    srcs = [
        "__init__.py",
        "release_tools.py",
    ],
    imports = ["../.."],
    srcs_version = "PY3",
)

py_binary(
    name = "print_rel_notes",
    srcs = [
        "print_rel_notes.py",
    ],
    imports = ["../.."],
    python_version = "PY3",
    deps = [
        ":release_utils",
    ],
)

py_test(
    name = "release_tools_test",
    srcs = ["release_tools_test.py"],
    python_version = "PY3",
    deps = [
        ":release_utils",
    ],
)

# This is an internal tool. Use at your own risk.
py_binary(
    name = "git_changelog_private",
    srcs = [
        "git_changelog_private.py",
    ],
    srcs_version = "PY3",
    # TODO(https://github.com/bazelbuild/bazel/issues/7377): Make this private.
    visibility = ["//visibility:public"],
)
