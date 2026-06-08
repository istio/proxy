# Copyright 2020 The Bazel Authors. All rights reserved.
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
"""toolchain to provide an rpmbuild binary."""

RpmbuildInfo = provider(
    doc = """Platform inde artifact.""",
    fields = {
        "name": "The name of the toolchain",
        "valid": "Is this toolchain valid and usable?",
        "label": "The path to a target I will build",
        "path": "The path to a pre-built rpmbuild",
        "version": "The version string of rpmbuild",
        "debuginfo_type": "The variant of the underlying debuginfo config",
    },
)

def _rpmbuild_toolchain_impl(ctx):
    if ctx.attr.label and ctx.attr.path:
        fail("rpmbuild_toolchain must not specify both label and path.")
    valid = bool(ctx.attr.label) or bool(ctx.attr.path)
    toolchain_info = platform_common.ToolchainInfo(
        rpmbuild = RpmbuildInfo(
            name = str(ctx.label),
            valid = valid,
            label = ctx.attr.label,
            path = ctx.attr.path,
            version = ctx.attr.version,
            debuginfo_type = ctx.attr.debuginfo_type,
        ),
    )
    return [toolchain_info]

rpmbuild_toolchain = rule(
    implementation = _rpmbuild_toolchain_impl,
    attrs = {
        "label": attr.label(
            doc = "A valid label of a target to build or a prebuilt binary. Mutually exclusive with path.",
            cfg = "exec",
            executable = True,
            allow_files = True,
        ),
        "path": attr.string(
            doc = "The path to the rpmbuild executable. Mutually exclusive with label.",
        ),
        "version": attr.string(
            doc = "The version string of the rpmbuild executable. This should be manually set.",
        ),
        "debuginfo_type": attr.string(
            doc = """
            The underlying debuginfo configuration for the system rpmbuild.

            One of `centos`, `fedora`, and `none`
            """,
            default = "none",
        ),
    },
)

# Expose the presence of an rpmbuild in the resolved toolchain as a flag.
def _is_rpmbuild_available_impl(ctx):
    toolchain = ctx.toolchains["@rules_pkg//toolchains/rpm:rpmbuild_toolchain_type"].rpmbuild
    return [config_common.FeatureFlagInfo(
        value = ("1" if toolchain.valid else "0"),
    )]

is_rpmbuild_available = rule(
    implementation = _is_rpmbuild_available_impl,
    attrs = {},
    toolchains = ["@rules_pkg//toolchains/rpm:rpmbuild_toolchain_type"],
)

# buildifier: disable=unnamed-macro
def rpmbuild_register_toolchains():
    native.register_toolchains("@rules_pkg//toolchains/rpm:rpmbuild_missing_toolchain")
