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
"""Implementation of the cc_tool_capability rule."""

load(
    ":cc_toolchain_info.bzl",
    "ArgsListInfo",
    "FeatureConstraintInfo",
    "FeatureInfo",
    "ToolCapabilityInfo",
)

def _cc_tool_capability_impl(ctx):
    ft = FeatureInfo(
        name = ctx.attr.feature_name or ctx.label.name,
        label = ctx.label,
        enabled = False,
        args = ArgsListInfo(
            label = ctx.label,
            args = (),
            files = depset(),
            by_action = (),
            allowlist_include_directories = depset(),
        ),
        implies = depset(),
        requires_any_of = (),
        mutually_exclusive = (),
        # Mark it as external so that it doesn't complain if we say
        # "requires" on a constraint that was never referenced elsewhere
        # in the toolchain.
        external = True,
        overridable = True,
        overrides = None,
        allowlist_include_directories = depset(),
    )
    return [
        ToolCapabilityInfo(label = ctx.label, feature = ft),
        # Only give it a feature constraint info and not a feature info.
        # This way you can't imply it - you can only require it.
        FeatureConstraintInfo(label = ctx.label, all_of = depset([ft])),
    ]

cc_tool_capability = rule(
    implementation = _cc_tool_capability_impl,
    provides = [ToolCapabilityInfo, FeatureConstraintInfo],
    doc = """A capability is an optional feature that a tool supports.

For example, not all compilers support PIC, so to handle this, we write:

```
cc_tool(
    name = "clang",
    src = "@host_tools/bin/clang",
    capabilities = [
        "//cc/toolchains/capabilities:supports_pic",
    ],
)

cc_args(
    name = "pic",
    requires = [
        "//cc/toolchains/capabilities:supports_pic"
    ],
    args = ["-fPIC"],
)
```

This ensures that `-fPIC` is added to the command-line only when we are using a
tool that supports PIC.
""",
    attrs = {
        "feature_name": attr.string(doc = "The name of the feature to generate for this capability"),
    },
)
