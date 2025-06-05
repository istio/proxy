# Copyright 2023 The Bazel Authors. All rights reserved.
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

"""
A starlark implementation of a Wheel filename parsing.
"""

# Taken from https://peps.python.org/pep-0600/
_LEGACY_ALIASES = {
    "manylinux1_i686": "manylinux_2_5_i686",
    "manylinux1_x86_64": "manylinux_2_5_x86_64",
    "manylinux2010_i686": "manylinux_2_12_i686",
    "manylinux2010_x86_64": "manylinux_2_12_x86_64",
    "manylinux2014_aarch64": "manylinux_2_17_aarch64",
    "manylinux2014_armv7l": "manylinux_2_17_armv7l",
    "manylinux2014_i686": "manylinux_2_17_i686",
    "manylinux2014_ppc64": "manylinux_2_17_ppc64",
    "manylinux2014_ppc64le": "manylinux_2_17_ppc64le",
    "manylinux2014_s390x": "manylinux_2_17_s390x",
    "manylinux2014_x86_64": "manylinux_2_17_x86_64",
}

def normalize_platform_tag(tag):
    """Resolve legacy aliases to modern equivalents for easier parsing elsewhere."""
    return ".".join(list({
        # The `list({})` usage here is to use it as a string set, where we will
        # deduplicate, but otherwise retain the order of the tags.
        _LEGACY_ALIASES.get(p, p): None
        for p in tag.split(".")
    }))

def parse_whl_name(file):
    """Parse whl file name into a struct of constituents.

    Args:
        file (str): The file name of a wheel

    Returns:
        A struct with the following attributes:
            distribution: the distribution name
            version: the version of the distribution
            build_tag: the build tag for the wheel. None if there was no
              build_tag in the given string.
            python_tag: the python tag for the wheel
            abi_tag: the ABI tag for the wheel
            platform_tag: the platform tag
    """
    if not file.endswith(".whl"):
        fail("not a valid wheel: {}".format(file))

    file = file[:-len(".whl")]

    # Parse the following
    # {distribution}-{version}(-{build tag})?-{python tag}-{abi tag}-{platform tag}.whl
    #
    # For more info, see the following standards:
    # https://packaging.python.org/en/latest/specifications/binary-distribution-format/#binary-distribution-format
    # https://packaging.python.org/en/latest/specifications/platform-compatibility-tags/
    head, _, platform_tag = file.rpartition("-")
    if not platform_tag:
        fail("cannot extract platform tag from the whl filename: {}".format(file))
    head, _, abi_tag = head.rpartition("-")
    if not abi_tag:
        fail("cannot extract abi tag from the whl filename: {}".format(file))
    head, _, python_tag = head.rpartition("-")
    if not python_tag:
        fail("cannot extract python tag from the whl filename: {}".format(file))
    head, _, version = head.rpartition("-")
    if not version:
        fail("cannot extract version from the whl filename: {}".format(file))
    distribution, _, maybe_version = head.partition("-")

    if maybe_version:
        version, build_tag = maybe_version, version
    else:
        build_tag = None

    return struct(
        distribution = distribution,
        version = version,
        build_tag = build_tag,
        python_tag = python_tag,
        abi_tag = abi_tag,
        platform_tag = normalize_platform_tag(platform_tag),
    )
