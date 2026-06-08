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
"""Rules to turn action types into bazel labels."""

load("//cc/toolchains/impl:collect.bzl", "collect_action_types")
load(":cc_toolchain_info.bzl", "ActionTypeInfo", "ActionTypeSetInfo")

visibility("public")

def _cc_action_type_impl(ctx):
    action_type = ActionTypeInfo(label = ctx.label, name = ctx.attr.action_name)
    return [
        action_type,
        ActionTypeSetInfo(
            label = ctx.label,
            actions = depset([action_type]),
        ),
    ]

cc_action_type = rule(
    implementation = _cc_action_type_impl,
    attrs = {
        "action_name": attr.string(
            mandatory = True,
        ),
    },
    doc = """A type of action (eg. c_compile, assemble, strip).

`cc_action_type` rules are used to associate arguments and tools together to
perform a specific action. Bazel prescribes a set of known action types that are used to drive
typical C/C++/ObjC actions like compiling, linking, and archiving. The set of well-known action
types can be found in [@rules_cc//cc/toolchains/actions:BUILD](https://github.com/bazelbuild/rules_cc/tree/main/cc/toolchains/actions/BUILD).

It's possible to create project-specific action types for use in toolchains. Be careful when
doing this, because every toolchain that encounters the action will need to be configured to
support the custom action type. If your project is a library, avoid creating new action types as
it will reduce compatibility with existing toolchains and increase setup complexity for users.

Example:
```
load("//cc:action_names.bzl", "ACTION_NAMES")
load("//cc/toolchains:actions.bzl", "cc_action_type")

cc_action_type(
    name = "cpp_compile",
    action_name =  = ACTION_NAMES.cpp_compile,
)
```
""",
    provides = [ActionTypeInfo, ActionTypeSetInfo],
)

def _cc_action_type_set_impl(ctx):
    if not ctx.attr.actions and not ctx.attr.allow_empty:
        fail("Each cc_action_type_set must contain at least one action type.")
    return [ActionTypeSetInfo(
        label = ctx.label,
        actions = collect_action_types(ctx.attr.actions),
    )]

cc_action_type_set = rule(
    doc = """Represents a set of actions.

This is a convenience rule to allow for more compact representation of a group of action types.
Use this anywhere a `cc_action_type` is accepted.

Example:
```
load("//cc/toolchains:actions.bzl", "cc_action_type_set")

cc_action_type_set(
    name = "link_executable_actions",
    actions = [
        "//cc/toolchains/actions:cpp_link_executable",
        "//cc/toolchains/actions:lto_index_for_executable",
    ],
)
```
""",
    implementation = _cc_action_type_set_impl,
    attrs = {
        "actions": attr.label_list(
            providers = [ActionTypeSetInfo],
            mandatory = True,
            doc = "A list of cc_action_type or cc_action_type_set",
        ),
        "allow_empty": attr.bool(default = False),
    },
    provides = [ActionTypeSetInfo],
)
