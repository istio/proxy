# Copyright 2023 The Bazel Authors. All rights reserved.
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

"""A small helper to ensure that we are working with full versions."""

def full_version(*, version, minor_mapping, fail_on_err = True):
    """Return a full version.

    Args:
        version: {type}`str` the version in `X.Y` or `X.Y.Z` format.
        minor_mapping: {type}`dict[str, str]` mapping between `X.Y` to `X.Y.Z` format.
        fail_on_err: {type}`bool` whether to fail on error or return `None` instead.

    Returns:
        a full version given the version string. If the string is already a
        major version then we return it as is.
    """
    if version in minor_mapping:
        return minor_mapping[version]

    parts = version.split(".")
    if len(parts) == 3:
        return version
    elif not fail_on_err:
        return None
    elif len(parts) == 2:
        fail(
            "Unknown Python version '{}', available values are: {}".format(
                version,
                ",".join(minor_mapping.keys()),
            ),
        )
    else:
        fail("Unknown version format: '{}'".format(version))
