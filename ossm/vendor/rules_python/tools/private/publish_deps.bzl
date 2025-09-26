# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""A simple macro to lock the requirements for twine
"""

load("//python/uv/private:lock.bzl", "lock")  # buildifier: disable=bzl-visibility

def publish_deps(*, name, args, outs, **kwargs):
    """Generate all of the requirements files for all platforms.

    Args:
        name: {type}`str`: the currently unused.
        args: {type}`list[str]`: the common args to apply.
        outs: {type}`dict[Label, str]`: the output files mapping to the platform
            for each requirement file to be generated.
        **kwargs: Extra args passed to the {rule}`lock` rule.
    """
    all_args = args
    for out, platform in outs.items():
        args = [] + all_args
        if platform:
            args.append("--python-platform=" + platform)
        else:
            args.append("--universal")

        lock(
            name = out.replace(".txt", ""),
            out = out,
            args = args,
            **kwargs
        )
