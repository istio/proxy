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
"""Rule for mutually exclusive categories in the rule based toolchain."""

load(":cc_toolchain_info.bzl", "MutuallyExclusiveCategoryInfo")

def _cc_mutually_exclusive_category_impl(ctx):
    return [MutuallyExclusiveCategoryInfo(
        label = ctx.label,
        name = str(ctx.label),
    )]

cc_mutually_exclusive_category = rule(
    implementation = _cc_mutually_exclusive_category_impl,
    doc = """A rule used to categorize `cc_feature` definitions for which only one can be enabled.

This is used by [`cc_feature.mutually_exclusive`](#cc_feature-mutually_exclusive) to express groups
of `cc_feature` definitions that are inherently incompatible with each other and must be treated as
mutually exclusive.

Warning: These groups are keyed by name, so two `cc_mutually_exclusive_category` definitions of the
same name in different packages will resolve to the same logical group.

Example:
```
load("//cc/toolchains:feature.bzl", "cc_feature")
load("//cc/toolchains:mutually_exclusive_category.bzl", "cc_mutually_exclusive_category")

cc_mutually_exclusive_category(
    name = "opt_level",
)

cc_feature(
    name = "speed_optimized",
    mutually_exclusive = [":opt_level"],
)

cc_feature(
    name = "size_optimized",
    mutually_exclusive = [":opt_level"],
)

cc_feature(
    name = "unoptimized",
    mutually_exclusive = [":opt_level"],
)
```
""",
    attrs = {},
    provides = [MutuallyExclusiveCategoryInfo],
)
