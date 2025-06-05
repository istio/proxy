# Copyright 2024 The Bazel Authors. All rights reserved.
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

"A semver version parser"

def _key(version):
    return (
        version.major,
        version.minor or 0,
        version.patch or 0,
        # non pre-release versions are higher
        version.pre_release == "",
        # then we compare each element of the pre_release tag separately
        tuple([
            (
                i if not i.isdigit() else "",
                # digit values take precedence
                int(i) if i.isdigit() else 0,
            )
            for i in version.pre_release.split(".")
        ]) if version.pre_release else None,
        # And build info is just alphabetic
        version.build,
    )

def _to_dict(self):
    return {
        "build": self.build,
        "major": self.major,
        "minor": self.minor,
        "patch": self.patch,
        "pre_release": self.pre_release,
    }

def semver(version):
    """Parse the semver version and return the values as a struct.

    Args:
        version: {type}`str` the version string.

    Returns:
        A {type}`struct` with `major`, `minor`, `patch` and `build` attributes.
    """

    # Implement the https://semver.org/ spec
    major, _, tail = version.partition(".")
    minor, _, tail = tail.partition(".")
    patch, _, build = tail.partition("+")
    patch, _, pre_release = patch.partition("-")

    # buildifier: disable=uninitialized
    self = struct(
        major = int(major),
        minor = int(minor) if minor.isdigit() else None,
        # NOTE: this is called `micro` in the Python interpreter versioning scheme
        patch = int(patch) if patch.isdigit() else None,
        pre_release = pre_release,
        build = build,
        # buildifier: disable=uninitialized
        key = lambda: _key(self),
        str = lambda: version,
        to_dict = lambda: _to_dict(self),
    )
    return self
