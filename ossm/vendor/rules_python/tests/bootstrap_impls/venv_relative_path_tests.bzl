# Copyright 2023 The Bazel Authors. All rights reserved.
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

"Unit tests for relative_path computation"

load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("//python/private:py_executable.bzl", "relative_path")  # buildifier: disable=bzl-visibility

_tests = []

def _relative_path_test(env):
    # Basic test cases

    env.expect.that_str(
        relative_path(
            from_ = "a/b",
            to = "c/d",
        ),
    ).equals("../../c/d")

    env.expect.that_str(
        relative_path(
            from_ = "a/b/c",
            to = "a/d",
        ),
    ).equals("../../d")
    env.expect.that_str(
        relative_path(
            from_ = "a/b/c",
            to = "a/b/c/d/e",
        ),
    ).equals("d/e")

    # Real examples

    # external py_binary uses external python runtime
    env.expect.that_str(
        relative_path(
            from_ = "other_repo~/python/private/_py_console_script_gen_py.venv/bin",
            to = "rules_python~~python~python_3_9_x86_64-unknown-linux-gnu/bin/python3",
        ),
    ).equals(
        "../../../../../rules_python~~python~python_3_9_x86_64-unknown-linux-gnu/bin/python3",
    )

    # internal py_binary uses external python runtime
    env.expect.that_str(
        relative_path(
            from_ = "_main/test/version_default.venv/bin",
            to = "rules_python~~python~python_3_9_x86_64-unknown-linux-gnu/bin/python3",
        ),
    ).equals(
        "../../../../rules_python~~python~python_3_9_x86_64-unknown-linux-gnu/bin/python3",
    )

    # external py_binary uses internal python runtime
    env.expect.that_str(
        relative_path(
            from_ = "other_repo~/python/private/_py_console_script_gen_py.venv/bin",
            to = "_main/python/python_3_9_x86_64-unknown-linux-gnu/bin/python3",
        ),
    ).equals(
        "../../../../../_main/python/python_3_9_x86_64-unknown-linux-gnu/bin/python3",
    )

    # internal py_binary uses internal python runtime
    env.expect.that_str(
        relative_path(
            from_ = "_main/scratch/main.venv/bin",
            to = "_main/python/python_3_9_x86_64-unknown-linux-gnu/bin/python3",
        ),
    ).equals(
        "../../../python/python_3_9_x86_64-unknown-linux-gnu/bin/python3",
    )

_tests.append(_relative_path_test)

def relative_path_test_suite(*, name):
    test_suite(name = name, basic_tests = _tests)
