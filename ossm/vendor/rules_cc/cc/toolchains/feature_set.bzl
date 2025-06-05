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
"""Implementation of the cc_feature_set rule."""

load("//cc/toolchains/impl:collect.bzl", "collect_features")
load(
    ":cc_toolchain_info.bzl",
    "FeatureConstraintInfo",
    "FeatureSetInfo",
)

def _cc_feature_set_impl(ctx):
    if ctx.attr.features:
        fail("Features is a reserved attribute in bazel. cc_feature_set takes `all_of` instead.")
    features = collect_features(ctx.attr.all_of)
    return [
        FeatureSetInfo(label = ctx.label, features = features),
        FeatureConstraintInfo(
            label = ctx.label,
            all_of = features,
            none_of = depset([]),
        ),
    ]

cc_feature_set = rule(
    implementation = _cc_feature_set_impl,
    attrs = {
        "all_of": attr.label_list(
            providers = [FeatureSetInfo],
            doc = "A set of features",
        ),
    },
    provides = [FeatureSetInfo],
    doc = """Defines a set of features.

This may be used by both `cc_feature` and `cc_args` rules, and is effectively a way to express
a logical `AND` operation across multiple required features.

Example:
```
load("//cc/toolchains:feature_set.bzl", "cc_feature_set")

cc_feature_set(
    name = "thin_lto_requirements",
    all_of = [
        ":thin_lto",
        ":opt",
    ],
)
```
""",
)
