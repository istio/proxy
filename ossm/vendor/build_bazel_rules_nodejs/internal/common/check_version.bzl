# Copyright 2017 The Bazel Authors. All rights reserved.
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

"""Parse version string utility function
"""

# From https://github.com/tensorflow/tensorflow/blob/5541ef4fbba56cf8930198373162dd3119e6ee70/tensorflow/workspace.bzl#L28

def parse_version(version_string):
    """
    Parse the version from a string

    The format handled is "<major>.<minor>.<patch>-<date> <commit>"

    Args:
      version_string: the version string to parse

    Returns:
      A 3-tuple of numbers: (<major>, <minor>, <patch>)
    """

    # Remove commit from version.
    version = version_string.split(" ", 1)[0]

    # Remove semver "build metadata" tag
    version = version.split("+", 1)[0]

    # Split into (release, date) parts and only return the release
    # as a tuple of integers.
    parts = version.split("-", 1)

    # Handle format x.x.xrcx
    parts = parts[0].split("rc", 1)

    # Turn "release" into a tuple of numbers
    version_tuple = ()
    for number in parts[0].split("."):
        version_tuple += (int(number),)
    return version_tuple

def check_version(current_version, minimum_version):
    """
    Verify that a version string greater or equal to a minimum version string.

    The format handled for the version strings is "<major>.<minor>.<patch>-<date> <commit>"

    Args:
      current_version: a string indicating the version to check
      minimum_version: a string indicating the minimum version

    Returns:
      True if current_version is greater or equal to the minimum_version, False otherwise
    """
    return parse_version(current_version) >= parse_version(minimum_version)

def check_version_range(current_version, minimum_version, maximum_version):
    """
    Verify that a version string >= minimum version and <= maximum version.

    The format handled for the version strings is "<major>.<minor>.<patch>-<date> <commit>"

    Args:
      current_version: a string indicating the version to check
      minimum_version: a string indicating the minimum version
      maximum_version: a string indicating the maximum version

    Returns:
      True if current_version >= minimum_version and <= maximum_version,
      False otherwise.
    """
    return (parse_version(current_version) >= parse_version(minimum_version) and
            parse_version(current_version) <= parse_version(maximum_version))
