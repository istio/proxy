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
"""Implementation of the cc_feature rule."""

load(
    "//cc/toolchains/impl:collect.bzl",
    "collect_args_lists",
    "collect_features",
    "collect_provider",
)
load(
    ":cc_toolchain_info.bzl",
    "ArgsListInfo",
    "FeatureConstraintInfo",
    "FeatureInfo",
    "FeatureSetInfo",
    "MutuallyExclusiveCategoryInfo",
)

def _cc_feature_impl(ctx):
    if bool(ctx.attr.feature_name) == (ctx.attr.overrides != None):
        fail("Exactly one of 'feature_name' and 'overrides' are required")

    if ctx.attr.overrides == None:
        overrides = None

        # In the future, we may consider making feature_name optional,
        # defaulting to ctx.label.name. However, starting that way would make it
        # very difficult if we did want to later change that.
        name = ctx.attr.feature_name
    else:
        overrides = ctx.attr.overrides[FeatureInfo]
        if not overrides.overridable:
            fail("Attempting to override %s, which is not overridable" % overrides.label)
        name = overrides.name

    # In the following scenario:
    # cc_args(name = "foo", env = {"FOO": "BAR"}, args = ["--foo"])
    # cc_action_config(name = "ac", args=[":foo"])

    # We will translate this into providers roughly equivalent to the following:
    # cc_args(name = "implied_by_ac_env", env = {"FOO": "BAR"}, args = ["--foo"])
    # cc_feature(name = "implied_by_ac", args = [":implied_by_ac_env"])
    # cc_action_config(
    #     name = "c_compile",
    #     implies = [":implied_by_c_compile"]
    # )

    # The reason for this is because although the legacy providers support
    # flag_sets in action_config, they don't support env sets.
    if name.startswith("implied_by_"):
        fail("Feature names starting with 'implied_by' are reserved")

    args = collect_args_lists(ctx.attr.args, ctx.label)
    feature = FeatureInfo(
        label = ctx.label,
        name = name,
        # Unused field, but leave it just in case we want to reuse it in the
        # future.
        enabled = False,
        args = args,
        implies = collect_features(ctx.attr.implies),
        requires_any_of = tuple(collect_provider(
            ctx.attr.requires_any_of,
            FeatureSetInfo,
        )),
        mutually_exclusive = tuple(collect_provider(
            ctx.attr.mutually_exclusive,
            MutuallyExclusiveCategoryInfo,
        )),
        external = False,
        overridable = False,
        overrides = overrides,
        allowlist_include_directories = args.allowlist_include_directories,
    )

    return [
        feature,
        FeatureSetInfo(label = ctx.label, features = depset([feature])),
        FeatureConstraintInfo(
            label = ctx.label,
            all_of = depset([feature]),
            none_of = depset([]),
        ),
        MutuallyExclusiveCategoryInfo(label = ctx.label, name = name),
    ]

cc_feature = rule(
    implementation = _cc_feature_impl,
    # @unsorted-dict-items
    attrs = {
        "feature_name": attr.string(
            doc = """The name of the feature that this rule implements.

The feature name is a string that will be used in the `features` attribute of
rules to enable them (eg. `cc_binary(..., features = ["opt"])`.

While two features with the same `feature_name` may not be bound to the same
toolchain, they can happily live alongside each other in the same BUILD file.

Example:
```
cc_feature(
    name = "sysroot_macos",
    feature_name = "sysroot",
    ...
)

cc_feature(
    name = "sysroot_linux",
    feature_name = "sysroot",
    ...
)
```
""",
        ),
        "args": attr.label_list(
            doc = """A list of `cc_args` or `cc_args_list` labels that are expanded when this feature is enabled.""",
            providers = [ArgsListInfo],
        ),
        "requires_any_of": attr.label_list(
            doc = """A list of feature sets that define toolchain compatibility.

If *at least one* of the listed `cc_feature_set`s are fully satisfied (all
features exist in the toolchain AND are currently enabled), this feature is
deemed compatible and may be enabled.

Note: Even if `cc_feature.requires_any_of` is satisfied, a feature is not
enabled unless another mechanism (e.g. command-line flags, `cc_feature.implies`,
`cc_toolchain_config.enabled_features`) signals that the feature should actually
be enabled.
""",
            providers = [FeatureSetInfo],
        ),
        "implies": attr.label_list(
            providers = [FeatureSetInfo],
            doc = """List of features enabled along with this feature.

Warning: If any of the features cannot be enabled, this feature is
silently disabled.
""",
        ),
        "mutually_exclusive": attr.label_list(
            providers = [MutuallyExclusiveCategoryInfo],
            doc = """A list of things that this feature is mutually exclusive with.

It can be either:
* A feature, in which case the two features are mutually exclusive.
* A `cc_mutually_exclusive_category`, in which case all features that write
    `mutually_exclusive = [":category"]` are mutually exclusive with each other.

If this feature has a side-effect of implementing another feature, it can be
useful to list that feature here to ensure they aren't enabled at the same time.
""",
        ),
        "overrides": attr.label(
            providers = [FeatureInfo],
            doc = """A declaration that this feature overrides a known feature.

In the example below, if you missed the "overrides" attribute, it would complain
that the feature "opt" was defined twice.

Example:
```
load("//cc/toolchains:feature.bzl", "cc_feature")

cc_feature(
    name = "opt",
    feature_name = "opt",
    args = [":size_optimized"],
    overrides = "//cc/toolchains/features:opt",
)
```
""",
        ),
    },
    provides = [
        FeatureInfo,
        FeatureSetInfo,
        FeatureConstraintInfo,
        MutuallyExclusiveCategoryInfo,
    ],
    doc = """A dynamic set of toolchain flags that create a singular [feature](https://bazel.build/docs/cc-toolchain-config-reference#features) definition.

A feature is basically a dynamically toggleable `cc_args_list`. There are a variety of
dependencies and compatibility requirements that must be satisfied to enable a
`cc_feature`. Once those conditions are met, the arguments in [`cc_feature.args`](#cc_feature-args)
are expanded and added to the command-line.

A feature may be enabled or disabled through the following mechanisms:
* Via command-line flags, or a `.bazelrc` file via the
  [`--features` flag](https://bazel.build/reference/command-line-reference#flag--features)
* Through inter-feature relationships (via [`cc_feature.implies`](#cc_feature-implies)) where one
  feature may implicitly enable another.
* Individual rules (e.g. `cc_library`) or `package` definitions may elect to manually enable or
  disable features through the
  [`features` attribute](https://bazel.build/reference/be/common-definitions#common.features).

Note that a feature may alternate between enabled and disabled dynamically over the course of a
build. Because of their toggleable nature, it's generally best to avoid adding arguments to a
`cc_toolchain` as a `cc_feature` unless strictly necessary. Instead, prefer to express arguments
via [`cc_toolchain.args`](#cc_toolchain-args) whenever possible.

You should use a `cc_feature` when any of the following apply:
* You need the flags to be dynamically toggled over the course of a build.
* You want build files to be able to configure the flags in question. For example, a
  binary might specify `features = ["optimize_for_size"]` to create a small
  binary instead of optimizing for performance.
* You need to carry forward Starlark toolchain behaviors. If you're migrating a
  complex Starlark-based toolchain definition to these rules, many of the
  workflows and flags were likely based on features.

If you only need to configure flags via the Bazel command-line, instead
consider adding a
[`bool_flag`](https://github.com/bazelbuild/bazel-skylib/tree/main/doc/common_settings_doc.md#bool_flag)
paired with a [`config_setting`](https://bazel.build/reference/be/general#config_setting)
and then make your `cc_args` rule `select` on the `config_setting`.

For more details about how Bazel handles features, see the official Bazel
documentation at
https://bazel.build/docs/cc-toolchain-config-reference#features.

Example:
```
load("//cc/toolchains:feature.bzl", "cc_feature")

# A feature that enables LTO, which may be incompatible when doing interop with various
# languages (e.g. rust, go), or may need to be disabled for particular `cc_binary` rules
# for various reasons.
cc_feature(
    name = "lto",
    feature_name = "lto",
    args = [":lto_args"],
)
```
""",
)
