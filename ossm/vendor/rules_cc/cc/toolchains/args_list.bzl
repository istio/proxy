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
"""All providers for rule-based bazel toolchain config."""

load(
    "//cc/toolchains/impl:collect.bzl",
    "collect_args_lists",
)
load(":cc_toolchain_info.bzl", "ArgsListInfo")

def _cc_args_list_impl(ctx):
    return [collect_args_lists(ctx.attr.args, ctx.label)]

cc_args_list = rule(
    implementation = _cc_args_list_impl,
    doc = """An ordered list of cc_args.

    This is a convenience rule to allow you to group a set of multiple `cc_args` into a
    single list. This particularly useful for toolchain behaviors that require different flags for
    different actions.

    Note: The order of the arguments in `args` is preserved to support order-sensitive flags.

    Example usage:
    ```
    load("//cc/toolchains:cc_args.bzl", "cc_args")
    load("//cc/toolchains:args_list.bzl", "cc_args_list")

    cc_args(
        name = "gc_sections",
        actions = [
            "//cc/toolchains/actions:link_actions",
        ],
        args = ["-Wl,--gc-sections"],
    )

    cc_args(
        name = "function_sections",
        actions = [
            "//cc/toolchains/actions:compile_actions",
            "//cc/toolchains/actions:link_actions",
        ],
        args = ["-ffunction-sections"],
    )

    cc_args_list(
        name = "gc_functions",
        args = [
            ":function_sections",
            ":gc_sections",
        ],
    )
    ```
    """,
    attrs = {
        "args": attr.label_list(
            providers = [ArgsListInfo],
            doc = "(ordered) cc_args to include in this list.",
        ),
    },
    provides = [ArgsListInfo],
)
