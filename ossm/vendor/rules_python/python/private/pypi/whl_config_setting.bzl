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

def whl_config_setting(*, version = None, target_platforms = None):
    """The bzl_packages value used by by the render_pkg_aliases function.

    This contains the minimum amount of information required to generate correct
    aliases in a hub repository.

    Args:
        version: {type}`str | None`the version of the python toolchain that this
            whl alias is for. If not set, then non-version aware aliases will be
            constructed. This is mainly used for better error messages when there
            is no match found during a select.
        target_platforms: {type}`list[str] | None` the list of target_platforms for this
            distribution.

    Returns:
        a struct with the validated and parsed values.
    """

    # FIXME @aignas 2025-07-26: There is still a potential that there will be ambigous match
    # if the user is trying to have different packages for 3.X.Y and 3.X.Z versions of python
    # in the same hub repository. Consider just removing this processing as I am not sure it
    # has much value.
    if target_platforms:
        target_platforms_input = target_platforms
        target_platforms = []
        for p in target_platforms_input:
            if not p.startswith("cp"):
                fail("target_platform should start with 'cp' denoting the python version, got: " + p)

            abi, _, tail = p.partition("_")

            # drop the micro version here, currently there is no usecase to use
            # multiple python interpreters with the same minor version but
            # different micro version.
            abi, _, _ = abi.partition(".")
            target_platforms.append("{}_{}".format(abi, tail))

    return struct(
        # Make the struct hashable
        target_platforms = tuple(target_platforms) if target_platforms else None,
        version = version,
    )
