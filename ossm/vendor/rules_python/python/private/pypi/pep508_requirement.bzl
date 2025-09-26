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

"""This module is for parsing PEP508 requires-dist and requirements lines.
"""

load("//python/private:normalize_name.bzl", "normalize_name")

_STRIP = ["(", " ", ">", "=", "<", "~", "!", "@"]

def requirement(spec):
    """Parse a PEP508 requirement line

    Args:
        spec: {type}`str` requirement line that will be parsed.

    Returns:
        A struct with the information.
    """
    spec = spec.strip()
    requires, _, maybe_hashes = spec.partition(";")

    version_start = requires.find("==")
    version = None
    if version_start != -1:
        # Extract everything after '==' until the next space or end of the string
        version, _, _ = requires[version_start + 2:].partition(" ")

        # Remove any trailing characters from the version string
        version = version.strip(" ")

    marker, _, _ = maybe_hashes.partition("--hash")
    requires, _, extras_unparsed = requires.partition("[")
    extras_unparsed, _, _ = extras_unparsed.partition("]")
    for char in _STRIP:
        requires, _, _ = requires.partition(char)
    extras = extras_unparsed.replace(" ", "").split(",")
    name = requires.strip(" ")
    name = normalize_name(name)

    return struct(
        name = name.replace("_", "-"),
        name_ = name,
        marker = marker.strip(" "),
        extras = extras,
        version = version,
    )
