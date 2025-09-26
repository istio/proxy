# Copyright 2024 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Tests for precompiling behavior."""

load("@rules_python_internal//:rules_python_config.bzl", rp_config = "config")
load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:truth.bzl", "matching")
load("@rules_testing//lib:util.bzl", rt_util = "util")
load("//python:py_binary.bzl", "py_binary")
load("//python:py_info.bzl", "PyInfo")
load("//python:py_library.bzl", "py_library")
load("//python:py_test.bzl", "py_test")
load("//tests/support:py_info_subject.bzl", "py_info_subject")
load(
    "//tests/support:support.bzl",
    "ADD_SRCS_TO_RUNFILES",
    "CC_TOOLCHAIN",
    "EXEC_TOOLS_TOOLCHAIN",
    "PRECOMPILE",
    "PY_TOOLCHAINS",
)

_COMMON_CONFIG_SETTINGS = {
    # This isn't enabled in all environments the tests run in, so disable
    # it for conformity.
    "//command_line_option:allow_unresolved_symlinks": True,
    "//command_line_option:extra_toolchains": [PY_TOOLCHAINS, CC_TOOLCHAIN],
    EXEC_TOOLS_TOOLCHAIN: "enabled",
}

_tests = []

def _test_executable_precompile_attr_enabled_setup(name, py_rule, **kwargs):
    if not rp_config.enable_pystar:
        rt_util.skip_test(name = name)
        return
    rt_util.helper_target(
        py_rule,
        name = name + "_subject",
        precompile = "enabled",
        srcs = ["main.py"],
        deps = [name + "_lib1"],
        **kwargs
    )
    rt_util.helper_target(
        py_library,
        name = name + "_lib1",
        srcs = ["lib1.py"],
        precompile = "enabled",
        deps = [name + "_lib2"],
    )

    # 2nd order target to verify propagation
    rt_util.helper_target(
        py_library,
        name = name + "_lib2",
        srcs = ["lib2.py"],
        precompile = "enabled",
    )
    analysis_test(
        name = name,
        impl = _test_executable_precompile_attr_enabled_impl,
        target = name + "_subject",
        config_settings = _COMMON_CONFIG_SETTINGS,
    )

def _test_executable_precompile_attr_enabled_impl(env, target):
    target = env.expect.that_target(target)
    runfiles = target.runfiles()
    runfiles_contains_at_least_predicates(runfiles, [
        matching.str_matches("__pycache__/main.fakepy-45.pyc"),
        matching.str_matches("__pycache__/lib1.fakepy-45.pyc"),
        matching.str_matches("__pycache__/lib2.fakepy-45.pyc"),
        matching.str_matches("/main.py"),
        matching.str_matches("/lib1.py"),
        matching.str_matches("/lib2.py"),
    ])

    target.default_outputs().contains_at_least_predicates([
        matching.file_path_matches("__pycache__/main.fakepy-45.pyc"),
        matching.file_path_matches("/main.py"),
    ])
    py_info = target.provider(PyInfo, factory = py_info_subject)
    py_info.direct_pyc_files().contains_exactly([
        "{package}/__pycache__/main.fakepy-45.pyc",
    ])
    py_info.transitive_pyc_files().contains_exactly([
        "{package}/__pycache__/main.fakepy-45.pyc",
        "{package}/__pycache__/lib1.fakepy-45.pyc",
        "{package}/__pycache__/lib2.fakepy-45.pyc",
    ])

def _test_precompile_enabled_py_binary(name):
    _test_executable_precompile_attr_enabled_setup(name = name, py_rule = py_binary, main = "main.py")

_tests.append(_test_precompile_enabled_py_binary)

def _test_precompile_enabled_py_test(name):
    _test_executable_precompile_attr_enabled_setup(name = name, py_rule = py_test, main = "main.py")

_tests.append(_test_precompile_enabled_py_test)

def _test_precompile_enabled_py_library_setup(name, impl, config_settings):
    if not rp_config.enable_pystar:
        rt_util.skip_test(name = name)
        return
    rt_util.helper_target(
        py_library,
        name = name + "_subject",
        srcs = ["lib.py"],
        precompile = "enabled",
    )
    analysis_test(
        name = name,
        impl = impl,  #_test_precompile_enabled_py_library_impl,
        target = name + "_subject",
        config_settings = _COMMON_CONFIG_SETTINGS | config_settings,
    )

def _test_precompile_enabled_py_library_common_impl(env, target):
    target = env.expect.that_target(target)

    target.default_outputs().contains_at_least_predicates([
        matching.file_path_matches("__pycache__/lib.fakepy-45.pyc"),
        matching.file_path_matches("/lib.py"),
    ])
    py_info = target.provider(PyInfo, factory = py_info_subject)
    py_info.direct_pyc_files().contains_exactly([
        "{package}/__pycache__/lib.fakepy-45.pyc",
    ])
    py_info.transitive_pyc_files().contains_exactly([
        "{package}/__pycache__/lib.fakepy-45.pyc",
    ])

def _test_precompile_enabled_py_library_add_to_runfiles_disabled(name):
    _test_precompile_enabled_py_library_setup(
        name = name,
        impl = _test_precompile_enabled_py_library_add_to_runfiles_disabled_impl,
        config_settings = {
            ADD_SRCS_TO_RUNFILES: "disabled",
        },
    )

def _test_precompile_enabled_py_library_add_to_runfiles_disabled_impl(env, target):
    _test_precompile_enabled_py_library_common_impl(env, target)
    runfiles = env.expect.that_target(target).runfiles()
    runfiles.contains_exactly([])

_tests.append(_test_precompile_enabled_py_library_add_to_runfiles_disabled)

def _test_precompile_enabled_py_library_add_to_runfiles_enabled(name):
    _test_precompile_enabled_py_library_setup(
        name = name,
        impl = _test_precompile_enabled_py_library_add_to_runfiles_enabled_impl,
        config_settings = {
            ADD_SRCS_TO_RUNFILES: "enabled",
        },
    )

def _test_precompile_enabled_py_library_add_to_runfiles_enabled_impl(env, target):
    _test_precompile_enabled_py_library_common_impl(env, target)
    runfiles = env.expect.that_target(target).runfiles()
    runfiles.contains_exactly([
        "{workspace}/{package}/lib.py",
    ])

_tests.append(_test_precompile_enabled_py_library_add_to_runfiles_enabled)

def _test_pyc_only(name):
    if not rp_config.enable_pystar:
        rt_util.skip_test(name = name)
        return
    rt_util.helper_target(
        py_binary,
        name = name + "_subject",
        precompile = "enabled",
        srcs = ["main.py"],
        main = "main.py",
        precompile_source_retention = "omit_source",
        pyc_collection = "include_pyc",
        deps = [name + "_lib"],
    )
    rt_util.helper_target(
        py_library,
        name = name + "_lib",
        srcs = ["lib.py"],
        precompile_source_retention = "omit_source",
    )
    analysis_test(
        name = name,
        impl = _test_pyc_only_impl,
        config_settings = _COMMON_CONFIG_SETTINGS | {
            PRECOMPILE: "enabled",
        },
        target = name + "_subject",
    )

_tests.append(_test_pyc_only)

def _test_pyc_only_impl(env, target):
    target = env.expect.that_target(target)
    runfiles = target.runfiles()
    runfiles.contains_predicate(
        matching.str_matches("/main.pyc"),
    )
    runfiles.contains_predicate(
        matching.str_matches("/lib.pyc"),
    )
    runfiles.not_contains_predicate(
        matching.str_endswith("/main.py"),
    )
    runfiles.not_contains_predicate(
        matching.str_endswith("/lib.py"),
    )
    target.default_outputs().contains_at_least_predicates([
        matching.file_path_matches("/main.pyc"),
    ])
    target.default_outputs().not_contains_predicate(
        matching.file_basename_equals("main.py"),
    )

def _test_precompiler_action(name):
    if not rp_config.enable_pystar:
        rt_util.skip_test(name = name)
        return
    rt_util.helper_target(
        py_binary,
        name = name + "_subject",
        srcs = ["main2.py"],
        main = "main2.py",
        precompile = "enabled",
        precompile_optimize_level = 2,
        precompile_invalidation_mode = "unchecked_hash",
    )
    analysis_test(
        name = name,
        impl = _test_precompiler_action_impl,
        target = name + "_subject",
        config_settings = _COMMON_CONFIG_SETTINGS,
    )

_tests.append(_test_precompiler_action)

def _test_precompiler_action_impl(env, target):
    action = env.expect.that_target(target).action_named("PyCompile")
    action.contains_flag_values([
        ("--optimize", "2"),
        ("--python_version", "4.5"),
        ("--invalidation_mode", "unchecked_hash"),
    ])
    action.has_flags_specified(["--src", "--pyc", "--src_name"])
    action.env().contains_at_least({
        "PYTHONHASHSEED": "0",
        "PYTHONNOUSERSITE": "1",
        "PYTHONSAFEPATH": "1",
    })

def _setup_precompile_flag_pyc_collection_attr_interaction(
        *,
        name,
        pyc_collection_attr,
        precompile_flag,
        test_impl):
    rt_util.helper_target(
        py_binary,
        name = name + "_bin",
        srcs = ["bin.py"],
        main = "bin.py",
        precompile = "disabled",
        pyc_collection = pyc_collection_attr,
        deps = [
            name + "_lib_inherit",
            name + "_lib_enabled",
            name + "_lib_disabled",
        ],
    )
    rt_util.helper_target(
        py_library,
        name = name + "_lib_inherit",
        srcs = ["lib_inherit.py"],
        precompile = "inherit",
    )
    rt_util.helper_target(
        py_library,
        name = name + "_lib_enabled",
        srcs = ["lib_enabled.py"],
        precompile = "enabled",
    )
    rt_util.helper_target(
        py_library,
        name = name + "_lib_disabled",
        srcs = ["lib_disabled.py"],
        precompile = "disabled",
    )
    analysis_test(
        name = name,
        impl = test_impl,
        target = name + "_bin",
        config_settings = _COMMON_CONFIG_SETTINGS | {
            PRECOMPILE: precompile_flag,
        },
    )

def _verify_runfiles(contains_patterns, not_contains_patterns):
    def _verify_runfiles_impl(env, target):
        runfiles = env.expect.that_target(target).runfiles()
        for pattern in contains_patterns:
            runfiles.contains_predicate(matching.str_matches(pattern))
        for pattern in not_contains_patterns:
            runfiles.not_contains_predicate(
                matching.str_matches(pattern),
            )

    return _verify_runfiles_impl

def _test_precompile_flag_enabled_pyc_collection_attr_include_pyc(name):
    if not rp_config.enable_pystar:
        rt_util.skip_test(name = name)
        return
    _setup_precompile_flag_pyc_collection_attr_interaction(
        name = name,
        precompile_flag = "enabled",
        pyc_collection_attr = "include_pyc",
        test_impl = _verify_runfiles(
            contains_patterns = [
                "__pycache__/lib_enabled.*.pyc",
                "__pycache__/lib_inherit.*.pyc",
            ],
            not_contains_patterns = [
                "/bin*.pyc",
                "/lib_disabled*.pyc",
            ],
        ),
    )

_tests.append(_test_precompile_flag_enabled_pyc_collection_attr_include_pyc)

# buildifier: disable=function-docstring-header
def _test_precompile_flag_enabled_pyc_collection_attr_disabled(name):
    """Verify that a binary can opt-out of using implicit pycs even when
    precompiling is enabled by default.
    """
    if not rp_config.enable_pystar:
        rt_util.skip_test(name = name)
        return
    _setup_precompile_flag_pyc_collection_attr_interaction(
        name = name,
        precompile_flag = "enabled",
        pyc_collection_attr = "disabled",
        test_impl = _verify_runfiles(
            contains_patterns = [
                "__pycache__/lib_enabled.*.pyc",
            ],
            not_contains_patterns = [
                "/bin*.pyc",
                "/lib_disabled*.pyc",
                "/lib_inherit.*.pyc",
            ],
        ),
    )

_tests.append(_test_precompile_flag_enabled_pyc_collection_attr_disabled)

# buildifier: disable=function-docstring-header
def _test_precompile_flag_disabled_pyc_collection_attr_include_pyc(name):
    """Verify that a binary can opt-in to using pycs even when precompiling is
    disabled by default."""
    if not rp_config.enable_pystar:
        rt_util.skip_test(name = name)
        return
    _setup_precompile_flag_pyc_collection_attr_interaction(
        name = name,
        precompile_flag = "disabled",
        pyc_collection_attr = "include_pyc",
        test_impl = _verify_runfiles(
            contains_patterns = [
                "__pycache__/lib_enabled.*.pyc",
                "__pycache__/lib_inherit.*.pyc",
            ],
            not_contains_patterns = [
                "/bin*.pyc",
                "/lib_disabled*.pyc",
            ],
        ),
    )

_tests.append(_test_precompile_flag_disabled_pyc_collection_attr_include_pyc)

def _test_precompile_flag_disabled_pyc_collection_attr_disabled(name):
    if not rp_config.enable_pystar:
        rt_util.skip_test(name = name)
        return
    _setup_precompile_flag_pyc_collection_attr_interaction(
        name = name,
        precompile_flag = "disabled",
        pyc_collection_attr = "disabled",
        test_impl = _verify_runfiles(
            contains_patterns = [
                "__pycache__/lib_enabled.*.pyc",
            ],
            not_contains_patterns = [
                "/bin*.pyc",
                "/lib_disabled*.pyc",
                "/lib_inherit.*.pyc",
            ],
        ),
    )

_tests.append(_test_precompile_flag_disabled_pyc_collection_attr_disabled)

# buildifier: disable=function-docstring-header
def _test_pyc_collection_disabled_library_omit_source(name):
    """Verify that, when a binary doesn't include implicit pyc files, libraries
    that set omit_source still have the py source file included.
    """
    if not rp_config.enable_pystar:
        rt_util.skip_test(name = name)
        return
    rt_util.helper_target(
        py_binary,
        name = name + "_subject",
        srcs = ["bin.py"],
        main = "bin.py",
        deps = [name + "_lib"],
        pyc_collection = "disabled",
    )
    rt_util.helper_target(
        py_library,
        name = name + "_lib",
        srcs = ["lib.py"],
        precompile = "inherit",
        precompile_source_retention = "omit_source",
    )
    analysis_test(
        name = name,
        impl = _test_pyc_collection_disabled_library_omit_source_impl,
        target = name + "_subject",
        config_settings = _COMMON_CONFIG_SETTINGS,
    )

def _test_pyc_collection_disabled_library_omit_source_impl(env, target):
    contains_patterns = [
        "/lib.py",
        "/bin.py",
    ]
    not_contains_patterns = [
        "/lib.*pyc",
        "/bin.*pyc",
    ]
    runfiles = env.expect.that_target(target).runfiles()
    for pattern in contains_patterns:
        runfiles.contains_predicate(matching.str_matches(pattern))
    for pattern in not_contains_patterns:
        runfiles.not_contains_predicate(
            matching.str_matches(pattern),
        )

_tests.append(_test_pyc_collection_disabled_library_omit_source)

def _test_pyc_collection_include_dep_omit_source(name):
    if not rp_config.enable_pystar:
        rt_util.skip_test(name = name)
        return
    rt_util.helper_target(
        py_binary,
        name = name + "_subject",
        srcs = ["bin.py"],
        main = "bin.py",
        deps = [name + "_lib"],
        precompile = "disabled",
        pyc_collection = "include_pyc",
    )
    rt_util.helper_target(
        py_library,
        name = name + "_lib",
        srcs = ["lib.py"],
        precompile = "inherit",
        precompile_source_retention = "omit_source",
    )
    analysis_test(
        name = name,
        impl = _test_pyc_collection_include_dep_omit_source_impl,
        target = name + "_subject",
        config_settings = _COMMON_CONFIG_SETTINGS,
    )

def _test_pyc_collection_include_dep_omit_source_impl(env, target):
    contains_patterns = [
        "/lib.pyc",
    ]
    not_contains_patterns = [
        "/lib.py",
    ]
    runfiles = env.expect.that_target(target).runfiles()
    for pattern in contains_patterns:
        runfiles.contains_predicate(matching.str_endswith(pattern))
    for pattern in not_contains_patterns:
        runfiles.not_contains_predicate(
            matching.str_endswith(pattern),
        )

_tests.append(_test_pyc_collection_include_dep_omit_source)

def _test_precompile_attr_inherit_pyc_collection_disabled_precompile_flag_enabled(name):
    if not rp_config.enable_pystar:
        rt_util.skip_test(name = name)
        return
    rt_util.helper_target(
        py_binary,
        name = name + "_subject",
        srcs = ["bin.py"],
        main = "bin.py",
        precompile = "inherit",
        pyc_collection = "disabled",
    )
    analysis_test(
        name = name,
        impl = _test_precompile_attr_inherit_pyc_collection_disabled_precompile_flag_enabled_impl,
        target = name + "_subject",
        config_settings = _COMMON_CONFIG_SETTINGS | {
            PRECOMPILE: "enabled",
        },
    )

def _test_precompile_attr_inherit_pyc_collection_disabled_precompile_flag_enabled_impl(env, target):
    target = env.expect.that_target(target)
    target.runfiles().not_contains_predicate(
        matching.str_matches("/bin.*pyc"),
    )
    target.default_outputs().not_contains_predicate(
        matching.file_path_matches("/bin.*pyc"),
    )

_tests.append(_test_precompile_attr_inherit_pyc_collection_disabled_precompile_flag_enabled)

def runfiles_contains_at_least_predicates(runfiles, predicates):
    for predicate in predicates:
        runfiles.contains_predicate(predicate)

def precompile_test_suite(name):
    test_suite(
        name = name,
        tests = _tests,
    )
