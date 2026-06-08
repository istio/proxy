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

"""A function to convert a dist name to a valid bazel repo name.
"""

load("//python/private:normalize_name.bzl", "normalize_name")
load(":parse_whl_name.bzl", "parse_whl_name")

def whl_repo_name(filename, sha256, *target_platforms):
    """Return a valid whl_library repo name given a distribution filename.

    Args:
        filename: {type}`str` the filename of the distribution.
        sha256: {type}`str` the sha256 of the distribution.
        *target_platforms: {type}`list[str]` the extra suffixes to append.
            Only used when we need to support different extras per version.

    Returns:
        a string that can be used in {obj}`whl_library`.
    """
    parts = []

    if not filename.endswith(".whl"):
        # Then the filename is basically foo-3.2.1.<ext>
        name, _, tail = filename.rpartition("-")
        parts.append(normalize_name(name))
        if sha256:
            parts.append("sdist")
            version = ""
        else:
            for ext in [".tar", ".zip"]:
                tail, _, _ = tail.partition(ext)
            version = tail.replace(".", "_").replace("!", "_")
    else:
        parsed = parse_whl_name(filename)
        name = normalize_name(parsed.distribution)
        version = parsed.version.replace(".", "_").replace("!", "_").replace("+", "_").replace("%", "_")
        python_tag, _, _ = parsed.python_tag.partition(".")
        abi_tag, _, _ = parsed.abi_tag.partition(".")
        platform_tag, _, _ = parsed.platform_tag.partition(".")

        parts.append(name)
        parts.append(python_tag)
        parts.append(abi_tag)
        parts.append(platform_tag)

    if sha256:
        parts.append(sha256[:8])
    elif version:
        parts.insert(1, version)

    parts.extend([p.partition("_")[-1] for p in target_platforms])

    return "_".join(parts)

def pypi_repo_name(whl_name, *target_platforms):
    """Return a valid whl_library given a requirement line.

    Args:
        whl_name: {type}`str` the whl_name to use.
        *target_platforms: {type}`list[str]` the target platforms to use in the name.

    Returns:
        {type}`str` that can be used in {obj}`whl_library`.
    """
    parts = [
        normalize_name(whl_name),
    ]
    parts.extend([p.partition("_")[-1] for p in target_platforms])

    return "_".join(parts)
