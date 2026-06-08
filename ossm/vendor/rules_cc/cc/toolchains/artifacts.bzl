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

"""Rules to turn artifact categories into targets."""

load(":cc_toolchain_info.bzl", "ArtifactCategoryInfo", "ArtifactNamePatternInfo")

visibility("public")

def _cc_artifact_name_pattern_impl(ctx):
    return [
        ArtifactNamePatternInfo(
            label = ctx.label,
            category = ctx.attr.category[ArtifactCategoryInfo],
            prefix = ctx.attr.prefix,
            extension = ctx.attr.extension,
        ),
    ]

cc_artifact_name_pattern = rule(
    implementation = _cc_artifact_name_pattern_impl,
    attrs = {
        "category": attr.label(
            mandatory = True,
            providers = [
                ArtifactCategoryInfo,
            ],
        ),
        "extension": attr.string(
            mandatory = True,
        ),
        "prefix": attr.string(
            mandatory = True,
        ),
    },
    doc = """
The name for an artifact of a given category of input or output artifacts to an
action.

This is used to declare that executables should follow `<name>.exe` on Windows,
or shared libraries should follow `lib<name>.dylib` on macOS.

Example:
```
load("//cc/toolchains:artifacts.bzl", "cc_artifact_name_pattern")

cc_artifact_name_pattern(
    name = "static_library",
    category = "//cc/toolchains/artifacts:static_library",
    prefix = "lib",
    extension = ".a",
)

cc_artifact_name_pattern(
    name = "executable",
    category = "//cc/toolchains/artifacts:executable",
    prefix = "",
    extension = "",
)
```
""",
    provides = [
        ArtifactNamePatternInfo,
    ],
)
