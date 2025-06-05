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

"A small function to create an alias for a whl distribution"

def whl_config_setting(*, version = None, config_setting = None, filename = None, target_platforms = None):
    """The bzl_packages value used by by the render_pkg_aliases function.

    This contains the minimum amount of information required to generate correct
    aliases in a hub repository.

    Args:
        version: optional(str), the version of the python toolchain that this
            whl alias is for. If not set, then non-version aware aliases will be
            constructed. This is mainly used for better error messages when there
            is no match found during a select.
        config_setting: optional(Label or str), the config setting that we should use. Defaults
            to "//_config:is_python_{version}".
        filename: optional(str), the distribution filename to derive the config_setting.
        target_platforms: optional(list[str]), the list of target_platforms for this
            distribution.

    Returns:
        a struct with the validated and parsed values.
    """
    if target_platforms:
        for p in target_platforms:
            if not p.startswith("cp"):
                fail("target_platform should start with 'cp' denoting the python version, got: " + p)

    return struct(
        config_setting = config_setting,
        filename = filename,
        # Make the struct hashable
        target_platforms = tuple(target_platforms) if target_platforms else None,
        version = version,
    )
