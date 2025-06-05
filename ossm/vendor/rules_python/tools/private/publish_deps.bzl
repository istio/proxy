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

def publish_deps(*, name, outs, **kwargs):
    """Generate all of the requirements files for all platforms."""
    for out, platform in outs.items():
        lock(
            name = out.replace(".txt", ""),
            out = out,
            universal = platform == "",
            args = [] if not platform else ["--python-platform=" + platform],
            **kwargs
        )
