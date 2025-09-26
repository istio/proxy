# Copyright 2025 The Bazel Authors. All rights reserved.
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

"""A macro used from the uv_toolchain hub repo."""

load(":toolchain_types.bzl", "UV_TOOLCHAIN_TYPE")

def toolchains_hub(
        *,
        name,
        toolchains,
        implementations,
        target_compatible_with,
        target_settings):
    """Define the toolchains so that the lexicographical order registration is deterministic.

    TODO @aignas 2025-03-09: see if this can be reused in the python toolchains.

    Args:
        name: The prefix to all of the targets, which goes after a numeric prefix.
        toolchains: The toolchain names for the targets defined by this macro.
            The earlier occurring items take precedence over the later items if
            they match the target platform and target settings.
        implementations: The name to label mapping.
        target_compatible_with: The name to target_compatible_with list mapping.
        target_settings: The name to target_settings list mapping.
    """
    if len(toolchains) != len(implementations):
        fail("Each name must have an implementation")

    # We are defining the toolchains so that the order of toolchain matching is
    # the same as the order of the toolchains, because:
    # * the toolchains are matched by target settings and target_compatible_with
    # * the first toolchain satisfying the above wins
    #
    # this means we need to register the toolchains prefixed with a number of
    # format 00xy, where x and y are some digits and the leading zeros to
    # ensure lexicographical sorting.
    #
    # Add 1 so that there is always a leading zero
    prefix_len = len(str(len(toolchains))) + 1
    prefix = "0" * (prefix_len - 1)

    for i, toolchain in enumerate(toolchains):
        # prefix with a prefix and then truncate the string.
        number_prefix = "{}{}".format(prefix, i)[-prefix_len:]

        native.toolchain(
            name = "{}_{}_{}".format(number_prefix, name, toolchain),
            target_compatible_with = target_compatible_with.get(toolchain, []),
            target_settings = target_settings.get(toolchain, []),
            toolchain = implementations[toolchain],
            toolchain_type = UV_TOOLCHAIN_TYPE,
        )
