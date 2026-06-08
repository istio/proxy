# Copyright 2019 The Bazel Authors. All rights reserved.
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

"""Common build definitions used by test fixtures."""

load("//swift:providers.bzl", "SwiftInfo")
load("//swift:swift_clang_module_aspect.bzl", "swift_clang_module_aspect")

# Common tags that prevent the test fixtures from actually being built (i.e.,
# their actions executed) when running `bazel test` to do analysis testing.
FIXTURE_TAGS = [
    "manual",
    "notap",
]

def _forward_swift_info_from_swift_clang_module_aspect_impl(ctx):
    if SwiftInfo in ctx.attr.target:
        return [ctx.attr.target[SwiftInfo]]
    return []

forward_swift_info_from_swift_clang_module_aspect = rule(
    attrs = {
        "target": attr.label(
            aspects = [swift_clang_module_aspect],
            mandatory = True,
        ),
    },
    doc = """\
Applies `swift_clang_module_aspect` to the given target and forwards the
`SwiftInfo` provider that it attaches to the target, if any.
""",
    implementation = _forward_swift_info_from_swift_clang_module_aspect_impl,
)
