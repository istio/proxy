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

"""
A file that houses private functions used in the `bzlmod` extension with the same name.
"""

def index_sources(line):
    """Get PyPI sources from a requirements.txt line.

    We interpret the spec described in
    https://pip.pypa.io/en/stable/reference/requirement-specifiers/#requirement-specifiers

    Args:
        line(str): The requirements.txt entry.

    Returns:
        A struct with shas attribute containing:
            * `shas` - list[str]; shas to download from pypi_index.
            * `version` - str; version of the package.
            * `marker` - str; the marker expression, as per PEP508 spec.
            * `requirement` - str; a requirement line without the marker. This can
                be given to `pip` to install a package.
    """
    line = line.replace("\\", " ")
    head, _, maybe_hashes = line.partition(";")
    _, _, version = head.partition("==")
    version = version.partition(" ")[0].strip()

    marker, _, _ = maybe_hashes.partition("--hash=")
    maybe_hashes = maybe_hashes or line
    shas = [
        sha.strip()
        for sha in maybe_hashes.split("--hash=sha256:")[1:]
    ]

    marker = marker.strip()
    if head == line:
        requirement = line.partition("--hash=")[0].strip()
    else:
        requirement = head.strip()

    requirement_line = "{} {}".format(
        requirement,
        " ".join(["--hash=sha256:{}".format(sha) for sha in shas]),
    ).strip()
    if "@" in head:
        requirement = requirement_line
        shas = []

    return struct(
        requirement = requirement,
        requirement_line = requirement_line,
        version = version,
        shas = sorted(shas),
        marker = marker,
    )
