# pylint: disable=g-bad-file-header
# Copyright 2016 The Bazel Authors. All rights reserved.
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
"""
Helpers for CC Toolchains.

Rules that require a CC toolchain should call `use_cc_toolchain` and `find_cc_toolchain`
to depend on and find a cc toolchain.

* When https://github.com/bazelbuild/bazel/issues/7260 is **not** flipped, current
  C++ toolchain is selected using the legacy mechanism (`--crosstool_top`,
  `--cpu`, `--compiler`). For that to work the rule needs to declare an
  `_cc_toolchain` attribute, e.g.

    foo = rule(
        implementation = _foo_impl,
        attrs = {
            "_cc_toolchain": attr.label(
                default = Label(
                    "@rules_cc//cc:current_cc_toolchain",
                ),
            ),
        },
    )

* When https://github.com/bazelbuild/bazel/issues/7260 **is** flipped, current
  C++ toolchain is selected using the toolchain resolution mechanism
  (`--platforms`). For that to work the rule needs to declare a dependency on
  C++ toolchain type:

    load(":find_cc_toolchain/bzl", "use_cc_toolchain")

    foo = rule(
        implementation = _foo_impl,
        toolchains = use_cc_toolchain(),
    )

We advise to depend on both `_cc_toolchain` attr and on the toolchain type for
the duration of the migration. After
https://github.com/bazelbuild/bazel/issues/7260 is flipped (and support for old
Bazel version is not needed), it's enough to only keep the toolchain type.
"""

load("//cc/common:cc_common.bzl", "cc_common")

CC_TOOLCHAIN_TYPE = Label("@bazel_tools//tools/cpp:toolchain_type")

CC_TOOLCHAIN_ATTRS = {
    # Needed for Bazel 6.x and 7.x compatibility.
    "_cc_toolchain": attr.label(default = Label("@rules_cc//cc:current_cc_toolchain")),
}

def find_cc_toolchain(ctx, *, mandatory = True):
    """
Returns the current `CcToolchainInfo`.

    Args:
      ctx: The rule context for which to find a toolchain.
      mandatory: (bool) If this is set to False, this function will return None
        rather than fail if no toolchain is found.

    Returns:
      A CcToolchainInfo or None if the c++ toolchain is declared as
      optional, mandatory is False and no toolchain has been found.
    """

    # Check the incompatible flag for toolchain resolution.
    if hasattr(cc_common, "is_cc_toolchain_resolution_enabled_do_not_use") and cc_common.is_cc_toolchain_resolution_enabled_do_not_use(ctx = ctx):
        if not CC_TOOLCHAIN_TYPE in ctx.toolchains:
            fail("In order to use find_cc_toolchain, your rule has to depend on C++ toolchain. See find_cc_toolchain.bzl docs for details.")
        toolchain_info = ctx.toolchains[CC_TOOLCHAIN_TYPE]
        if toolchain_info == None:
            if not mandatory:
                return None

            # No cpp toolchain was found, so report an error.
            fail("Unable to find a CC toolchain using toolchain resolution. Target: %s, Platform: %s, Exec platform: %s" %
                 (ctx.label, ctx.fragments.platform.platform, ctx.fragments.platform.host_platform))
        if hasattr(toolchain_info, "cc_provider_in_toolchain") and hasattr(toolchain_info, "cc"):
            return toolchain_info.cc
        return toolchain_info

    # Fall back to the legacy implicit attribute lookup.
    if hasattr(ctx.attr, "_cc_toolchain"):
        return ctx.attr._cc_toolchain[cc_common.CcToolchainInfo]

    # We didn't find anything.
    if not mandatory:
        return None
    fail("In order to use find_cc_toolchain, your rule has to depend on C++ toolchain. See find_cc_toolchain.bzl docs for details.")

def find_cpp_toolchain(ctx):
    """Deprecated, use `find_cc_toolchain` instead.

    Args:
      ctx: See `find_cc_toolchain`.

    Returns:
      A CcToolchainInfo.
    """
    return find_cc_toolchain(ctx)

def use_cc_toolchain(mandatory = False):
    """
    Helper to depend on the cc toolchain.

    Usage:
    ```
    my_rule = rule(
        toolchains = [other toolchain types] + use_cc_toolchain(),
    )
    ```

    Args:
      mandatory: Whether or not it should be an error if the toolchain cannot be resolved.

    Returns:
      A list that can be used as the value for `rule.toolchains`.
    """
    return [config_common.toolchain_type(CC_TOOLCHAIN_TYPE, mandatory = mandatory)]
