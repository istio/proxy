# Copyright 2019 The Bazel Authors. All rights reserved.
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

"""Support functions for working with Xcode configurations."""

def _is_xcode_at_least_version(xcode_config, version):
    """Returns True if Xcode version is at least a given version.

    This method takes as input an `XcodeVersionConfig` provider, which can be obtained from the
    `_xcode_config` attribute (e.g. `ctx.attr._xcode_config[apple_common.XcodeVersionConfig]`). This
    provider should contain the Xcode version parameters with which this rule is being built with.
    If you need to add this attribute to your rule implementation, please refer to
    `apple_support.action_required_attrs()`.

    Args:
        xcode_config: The XcodeVersionConfig provider from the `_xcode_config` attribute's value.
        version: The minimum desired Xcode version, as a dotted version string.

    Returns:
        True if the given `xcode_config` version at least as high as the requested version.
    """
    current_version = xcode_config.xcode_version()
    if not current_version:
        fail("Could not determine Xcode version at all. This likely means Xcode isn't available; " +
             "if you think this is a mistake, please file an issue.")

    desired_version = apple_common.dotted_version(version)
    return current_version >= desired_version

# Define the loadable module that lists the exported symbols in this file.
xcode_support = struct(
    is_xcode_at_least_version = _is_xcode_at_least_version,
)
