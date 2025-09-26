# Copyright 2023 The Bazel Authors. All rights reserved.
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
"""Helpers and utilities multiple tests re-use."""

load("@bazel_skylib//lib:structs.bzl", "structs")
load("//python/private:util.bzl", "IS_BAZEL_6_OR_HIGHER")  # buildifier: disable=bzl-visibility

# Use this with is_windows()
WINDOWS_ATTR = {"windows": attr.label(default = "@platforms//os:windows")}

def _create_tests(tests, **kwargs):
    test_names = []
    for func in tests:
        test_name = _test_name_from_function(func)
        func(name = test_name, **kwargs)
        test_names.append(test_name)
    return test_names

def _test_name_from_function(func):
    """Derives the name of the given rule implementation function.

    Args:
      func: the function whose name to extract

    Returns:
      The name of the given function. Note it will have leading and trailing
      "_" stripped -- this allows passing a private function and having the
      name of the test not start with "_".
    """

    # Starlark currently stringifies a function as "<function NAME>", so we use
    # that knowledge to parse the "NAME" portion out.
    # NOTE: This is relying on an implementation detail of Bazel
    func_name = str(func)
    func_name = func_name.partition("<function ")[-1]
    func_name = func_name.rpartition(">")[0]
    func_name = func_name.partition(" ")[0]
    return func_name.strip("_")

def _struct_with(s, **kwargs):
    struct_dict = structs.to_dict(s)
    struct_dict.update(kwargs)
    return struct(**struct_dict)

def _is_bazel_6_or_higher():
    return IS_BAZEL_6_OR_HIGHER

def _is_windows(env):
    """Tell if the target platform is windows.

    This assumes the `WINDOWS_ATTR` attribute was added.

    Args:
        env: The test env struct
    Returns:
        True if the target is Windows, False if not.
    """
    constraint = env.ctx.attr.windows[platform_common.ConstraintValueInfo]
    return env.ctx.target_platform_has_constraint(constraint)

util = struct(
    create_tests = _create_tests,
    struct_with = _struct_with,
    is_bazel_6_or_higher = _is_bazel_6_or_higher,
    is_windows = _is_windows,
)
