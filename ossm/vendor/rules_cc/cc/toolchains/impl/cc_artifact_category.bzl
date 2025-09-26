# Copyright 2025 The Bazel Authors. All rights reserved.
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

"""Rules to declare artifact categories as targets."""

load("//cc/toolchains:cc_toolchain_info.bzl", "ArtifactCategoryInfo")

visibility([
    "//cc/toolchains/...",
])

def _cc_artifact_category_impl(ctx):
    return [
        ArtifactCategoryInfo(
            label = ctx.label,
            name = ctx.attr.name,
        ),
    ]

cc_artifact_category = rule(
    implementation = _cc_artifact_category_impl,
    doc = """
An artifact category (eg. static_library, executable, object_file).

Example:
```
load("//cc/toolchains:artifacts.bzl", "cc_artifact_category")

cc_artifact_category(
    name = "static_library",
)
```
""",
    provides = [
        ArtifactCategoryInfo,
    ],
)
