# Copyright 2022 The Bazel Authors. All rights reserved.
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
"""Defs to implement tests."""

def _py_cc_toolchain_available_test_impl(ctx):
    toolchain = ctx.toolchains["@rules_python//python/cc:toolchain_type"]

    if toolchain == None:
        fail("expected @rules_python//python/cc:toolchain_type toolchain " +
             "to be found, but it was not found")

    executable = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.write(executable, "# no-op file", is_executable = True)
    return [DefaultInfo(
        executable = executable,
    )]

py_cc_toolchain_available_test = rule(
    implementation = _py_cc_toolchain_available_test_impl,
    toolchains = [
        config_common.toolchain_type(
            "@rules_python//python/cc:toolchain_type",
            mandatory = False,
        ),
    ],
    test = True,
)
