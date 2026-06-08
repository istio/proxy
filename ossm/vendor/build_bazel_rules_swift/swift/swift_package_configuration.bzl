# Copyright 2021 The Bazel Authors. All rights reserved.
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

"""Support for setting compiler configurations on a per-package basis."""

load("//swift/internal:package_specs.bzl", "parse_package_specs")
load(":providers.bzl", "SwiftPackageConfigurationInfo")

def _swift_package_configuration_impl(ctx):
    disabled_features = []
    enabled_features = []
    for feature_string in ctx.attr.configured_features:
        if feature_string.startswith("-"):
            disabled_features.append(feature_string[1:])
        else:
            enabled_features.append(feature_string)

    return [
        SwiftPackageConfigurationInfo(
            disabled_features = disabled_features,
            enabled_features = enabled_features,
            package_specs = parse_package_specs(
                package_specs = ctx.attr.packages,
                workspace_name = ctx.label.workspace_name,
            ),
        ),
    ]

swift_package_configuration = rule(
    attrs = {
        "configured_features": attr.string_list(
            default = [],
            doc = """\
A list of feature strings that will be applied by default to targets in the
packages matched by the `packages` attribute, as if they had been specified by
the `package(features = ...)` rule in the BUILD file.

This list may include both feature names and/or negations (a name with a leading
`-`); a regular feature name means that the targets in the matching packages
will have the feature enabled, and a negated feature means that the target will
have the feature disabled.

For example, `configured_features = ["foo", "-bar"]` means that targets in the
configuration's packages will have the feature `"foo"` enabled by default and
the feature `"bar"` disabled by default.
""",
            mandatory = False,
        ),
        "packages": attr.string_list(
            allow_empty = True,
            doc = """\
A list of strings representing packages (possibly recursive) whose targets will
have this package configuration applied. Each package pattern is written in the
syntax used by the `package_group` function:

*   `//foo/bar`: Targets in the package `//foo/bar` but not in subpackages.

*   `//foo/bar/...`: Targets in the package `//foo/bar` and any of its
    subpackages.

*   A leading `-` excludes packages that would otherwise have been included by
    the patterns in the list.

Exclusions always take priority over inclusions; order in the list is
irrelevant.
""",
            mandatory = True,
        ),
    },
    doc = """\
A compilation configuration to apply to the Swift targets in a set of packages.

A Swift toolchain target can reference any number (zero or more) of
`swift_package_configuration` targets. When the compilation action for a target
is being configured, those package configurations will be applied if the
target's label is included by the package specifications in the configuration.
""",
    implementation = _swift_package_configuration_impl,
)
