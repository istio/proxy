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
"""Tests for actions for the rule based toolchain."""

load(
    "//cc/toolchains:cc_toolchain_info.bzl",
    "ActionTypeInfo",
    "ActionTypeSetInfo",
)

visibility("private")

def _test_action_types_impl(env, targets):
    env.expect.that_target(targets.c_compile).provider(ActionTypeInfo) \
        .name().equals("c_compile")
    env.expect.that_target(targets.cpp_compile).provider(ActionTypeSetInfo) \
        .actions().contains_exactly([targets.cpp_compile.label])
    env.expect.that_target(targets.all_compile).provider(ActionTypeSetInfo) \
        .actions().contains_exactly([
        targets.c_compile.label,
        targets.cpp_compile.label,
    ])

TARGETS = [
    ":c_compile",
    ":cpp_compile",
    ":all_compile",
]

TESTS = {
    "actions_test": _test_action_types_impl,
}
