# Copyright 2024 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Setup a python-build-standalone based toolchain."""

load("@rules_cc//cc:cc_import.bzl", "cc_import")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//python:py_runtime.bzl", "py_runtime")
load("//python:py_runtime_pair.bzl", "py_runtime_pair")
load("//python/cc:py_cc_toolchain.bzl", "py_cc_toolchain")
load(":glob_excludes.bzl", "glob_excludes")
load(":py_exec_tools_toolchain.bzl", "py_exec_tools_toolchain")
load(":semver.bzl", "semver")

_IS_FREETHREADED = Label("//python/config_settings:is_py_freethreaded")

def define_hermetic_runtime_toolchain_impl(
        *,
        name,
        extra_files_glob_include,
        extra_files_glob_exclude,
        python_version,
        python_bin,
        coverage_tool):
    """Define a toolchain implementation for a python-build-standalone repo.

    It expected this macro is called in the top-level package of an extracted
    python-build-standalone repository. See
    python/private/python_repositories.bzl for how it is invoked.

    Args:
        name: {type}`str` name used for tools to identify the invocation.
        extra_files_glob_include: {type}`list[str]` additional glob include
            patterns for the target runtime files (the one included in
            binaries).
        extra_files_glob_exclude: {type}`list[str]` additional glob exclude
            patterns for the target runtime files.
        python_version: {type}`str` The Python version, in `major.minor.micro`
            format.
        python_bin: {type}`str` The path to the Python binary within the
            repository.
        coverage_tool: {type}`str` optional target to the coverage tool to
            use.
    """
    _ = name  # @unused
    version_info = semver(python_version)
    version_dict = version_info.to_dict()
    native.filegroup(
        name = "files",
        srcs = native.glob(
            include = [
                "bin/**",
                "extensions/**",
                "include/**",
                "libs/**",
                "share/**",
            ] + extra_files_glob_include,
            # Platform-agnostic filegroup can't match on all patterns.
            allow_empty = True,
            exclude = [
                # Unused shared libraries. `python` executable and the `:libpython` target
                # depend on `libpython{python_version}.so.1.0`.
                "lib/libpython{major}.{minor}*.so".format(**version_dict),
                # static libraries
                "lib/**/*.a",
                # tests for the standard libraries.
                "lib/python{major}.{minor}*/**/test/**".format(**version_dict),
                "lib/python{major}.{minor}*/**/tests/**".format(**version_dict),
                # During pyc creation, temp files named *.pyc.NNN are created
                "**/__pycache__/*.pyc.*",
            ] + glob_excludes.version_dependent_exclusions() + extra_files_glob_exclude,
        ),
    )
    cc_import(
        name = "interface",
        interface_library = select({
            _IS_FREETHREADED: "libs/python{major}{minor}t.lib".format(**version_dict),
            "//conditions:default": "libs/python{major}{minor}.lib".format(**version_dict),
        }),
        system_provided = True,
    )
    cc_import(
        name = "abi3_interface",
        interface_library = select({
            _IS_FREETHREADED: "libs/python3t.lib",
            "//conditions:default": "libs/python3.lib",
        }),
        system_provided = True,
    )

    native.filegroup(
        name = "includes",
        srcs = native.glob(["include/**/*.h"]),
    )
    cc_library(
        name = "python_headers",
        deps = select({
            "@bazel_tools//src/conditions:windows": [":interface", ":abi3_interface"],
            "//conditions:default": None,
        }),
        hdrs = [":includes"],
        includes = [
            "include",
        ] + select({
            _IS_FREETHREADED: [
                "include/python{major}.{minor}t".format(**version_dict),
            ],
            "//conditions:default": [
                "include/python{major}.{minor}".format(**version_dict),
                "include/python{major}.{minor}m".format(**version_dict),
            ],
        }),
    )
    native.config_setting(
        name = "is_freethreaded_linux",
        flag_values = {
            Label("//python/config_settings:py_freethreaded"): "yes",
        },
        constraint_values = [
            "@platforms//os:linux",
        ],
        visibility = ["//visibility:private"],
    )
    native.config_setting(
        name = "is_freethreaded_osx",
        flag_values = {
            Label("//python/config_settings:py_freethreaded"): "yes",
        },
        constraint_values = [
            "@platforms//os:osx",
        ],
        visibility = ["//visibility:private"],
    )
    native.config_setting(
        name = "is_freethreaded_windows",
        flag_values = {
            Label("//python/config_settings:py_freethreaded"): "yes",
        },
        constraint_values = [
            "@platforms//os:windows",
        ],
        visibility = ["//visibility:private"],
    )

    cc_library(
        name = "libpython",
        hdrs = [":includes"],
        srcs = select({
            ":is_freethreaded_linux": [
                "lib/libpython{major}.{minor}t.so".format(**version_dict),
                "lib/libpython{major}.{minor}t.so.1.0".format(**version_dict),
            ],
            ":is_freethreaded_osx": [
                "lib/libpython{major}.{minor}t.dylib".format(**version_dict),
            ],
            ":is_freethreaded_windows": [
                "python3t.dll",
                "python{major}{minor}t.dll".format(**version_dict),
                "libs/python{major}{minor}t.lib".format(**version_dict),
                "libs/python3t.lib",
            ],
            "@platforms//os:linux": [
                "lib/libpython{major}.{minor}.so".format(**version_dict),
                "lib/libpython{major}.{minor}.so.1.0".format(**version_dict),
            ],
            "@platforms//os:macos": ["lib/libpython{major}.{minor}.dylib".format(**version_dict)],
            "@platforms//os:windows": [
                "python3.dll",
                "python{major}{minor}.dll".format(**version_dict),
                "libs/python{major}{minor}.lib".format(**version_dict),
                "libs/python3.lib",
            ],
        }),
    )

    native.exports_files(["python", python_bin])

    # Used to only download coverage toolchain when the coverage is collected by
    # bazel.
    native.config_setting(
        name = "coverage_enabled",
        values = {"collect_code_coverage": "true"},
        visibility = ["//visibility:private"],
    )

    py_runtime(
        name = "py3_runtime",
        files = [":files"],
        interpreter = python_bin,
        interpreter_version_info = {
            "major": str(version_info.major),
            "micro": str(version_info.patch),
            "minor": str(version_info.minor),
        },
        coverage_tool = select({
            # Convert empty string to None
            ":coverage_enabled": coverage_tool or None,
            "//conditions:default": None,
        }),
        python_version = "PY3",
        implementation_name = "cpython",
        # See https://peps.python.org/pep-3147/ for pyc tag infix format
        pyc_tag = select({
            _IS_FREETHREADED: "cpython-{major}{minor}t".format(**version_dict),
            "//conditions:default": "cpython-{major}{minor}".format(**version_dict),
        }),
    )

    py_runtime_pair(
        name = "python_runtimes",
        py2_runtime = None,
        py3_runtime = ":py3_runtime",
    )

    py_cc_toolchain(
        name = "py_cc_toolchain",
        headers = ":python_headers",
        libs = ":libpython",
        python_version = python_version,
    )

    py_exec_tools_toolchain(
        name = "py_exec_tools_toolchain",
        # This macro is called in another repo: use Label() to ensure it
        # resolves in the rules_python context.
        precompiler = Label("//tools/precompiler:precompiler"),
    )
