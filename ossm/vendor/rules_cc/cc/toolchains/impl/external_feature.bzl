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
"""Implementation of the cc_external_feature rule."""

load(
    "//cc/toolchains:cc_toolchain_info.bzl",
    "ArgsListInfo",
    "FeatureConstraintInfo",
    "FeatureInfo",
    "FeatureSetInfo",
)

visibility([
    "//cc/toolchains/...",
    "//tests/rule_based_toolchain/...",
])

def _cc_external_feature_impl(ctx):
    feature = FeatureInfo(
        label = ctx.label,
        name = ctx.attr.feature_name,
        enabled = False,
        args = ArgsListInfo(
            label = ctx.label,
            args = (),
            files = depset([]),
            by_action = (),
        ),
        implies = depset([]),
        requires_any_of = (),
        mutually_exclusive = (),
        external = True,
        overridable = ctx.attr.overridable,
        overrides = None,
        allowlist_include_directories = depset(),
    )
    providers = [
        feature,
        FeatureSetInfo(label = ctx.label, features = depset([feature])),
        FeatureConstraintInfo(
            label = ctx.label,
            all_of = depset([feature]),
            none_of = depset([]),
        ),
    ]
    return providers

cc_external_feature = rule(
    implementation = _cc_external_feature_impl,
    attrs = {
        "feature_name": attr.string(
            mandatory = True,
            doc = "The name of the feature",
        ),
        "overridable": attr.bool(
            doc = "Whether the feature can be overridden",
            mandatory = True,
        ),
    },
    provides = [FeatureInfo, FeatureSetInfo, FeatureConstraintInfo],
    doc = """A declaration that a [feature](https://bazel.build/docs/cc-toolchain-config-reference#features) with this name is defined elsewhere.

This rule communicates that a feature has been defined externally to make it possible to reference
features that live outside the rule-based cc toolchain ecosystem. This allows various toolchain
rules to reference the external feature without accidentally re-defining said feature.

This rule is currently considered a private API of the toolchain rules to encourage the Bazel
ecosystem to migrate to properly defining their features as rules.

Example:
```
load("//cc/toolchains:external_feature.bzl", "cc_external_feature")

# rules_rust defines a feature that is disabled whenever rust artifacts are being linked using
# the cc toolchain to signal that incompatible flags should be disabled as well.
cc_external_feature(
    name = "rules_rust_unsupported_feature",
    feature_name = "rules_rust_unsupported_feature",
    overridable = False,
)
```
""",
)
