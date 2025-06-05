# Copyright 2018 The Bazel Authors. All rights reserved.
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

"""Skylib module containing functions for checking Bazel versions."""

def _get_bazel_version():
    """Returns the current Bazel version"""

    return native.bazel_version

def _extract_version_number(bazel_version):
    """Extracts the semantic version number from a version string

    Args:
      bazel_version: the version string that begins with the semantic version
        e.g. "1.2.3rc1 abc1234" where "abc1234" is a commit hash.

    Returns:
      The semantic version string, like "1.2.3".
    """
    for i in range(len(bazel_version)):
        c = bazel_version[i]
        if not (c.isdigit() or c == "."):
            return bazel_version[:i]
    return bazel_version

# Parse the bazel version string from `native.bazel_version`.
# e.g.
# "0.10.0rc1 abc123d" => (0, 10, 0)
# "0.3.0" => (0, 3, 0)
def _parse_bazel_version(bazel_version):
    """Parses a version string into a 3-tuple of ints

    int tuples can be compared directly using binary operators (<, >).

    For a development build of Bazel, this returns an unspecified version tuple
    that compares higher than any released version.

    Args:
      bazel_version: the Bazel version string

    Returns:
      An int 3-tuple of a (major, minor, patch) version.
    """

    version = _extract_version_number(bazel_version)
    if not version:
        return (999999, 999999, 999999)
    return tuple([int(n) for n in version.split(".")])

def _is_at_most(threshold, version):
    """Check that a version is lower or equals to a threshold.

    Args:
      threshold: the maximum version string
      version: the version string to be compared to the threshold

    Returns:
      True if version <= threshold.
    """
    return _parse_bazel_version(version) <= _parse_bazel_version(threshold)

def _is_at_least(threshold, version):
    """Check that a version is higher or equals to a threshold.

    Args:
      threshold: the minimum version string
      version: the version string to be compared to the threshold

    Returns:
      True if version >= threshold.
    """

    return _parse_bazel_version(version) >= _parse_bazel_version(threshold)

def _check_bazel_version(minimum_bazel_version, maximum_bazel_version = None, bazel_version = None):
    """Check that the version of Bazel is valid within the specified range.

    Args:
      minimum_bazel_version: minimum version of Bazel expected
      maximum_bazel_version: maximum version of Bazel expected
      bazel_version: the version of Bazel to check. Used for testing, defaults to native.bazel_version
    """
    if not bazel_version:
        if "bazel_version" not in dir(native):
            fail("Current Bazel version is lower than 0.2.1; expected at least {}".format(
                minimum_bazel_version,
            ))
        elif not native.bazel_version:
            # Using a non-release version, assume it is good.
            return
        else:
            bazel_version = native.bazel_version

    if not _is_at_least(
        threshold = minimum_bazel_version,
        version = bazel_version,
    ):
        fail("Current Bazel version is {}; expected at least {}".format(
            bazel_version,
            minimum_bazel_version,
        ))

    if maximum_bazel_version:
        if not _is_at_most(
            threshold = maximum_bazel_version,
            version = bazel_version,
        ):
            fail("Current Bazel version is {}; expected at most {}".format(
                bazel_version,
                maximum_bazel_version,
            ))

    pass

versions = struct(
    get = _get_bazel_version,
    parse = _parse_bazel_version,
    check = _check_bazel_version,
    is_at_most = _is_at_most,
    is_at_least = _is_at_least,
)
