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

"""
EXPERIMENTAL: This is experimental and may be removed without notice

A module extension for working with uv.
"""

load(":uv_repositories.bzl", "uv_repositories")

_DOC = """\
A module extension for working with uv.
"""

uv_toolchain = tag_class(
    doc = "Configure uv toolchain for lock file generation.",
    attrs = {
        "uv_version": attr.string(doc = "Explicit version of uv.", mandatory = True),
    },
)

def _uv_toolchain_extension(module_ctx):
    for mod in module_ctx.modules:
        for toolchain in mod.tags.toolchain:
            if not mod.is_root:
                fail(
                    "Only the root module may configure the uv toolchain.",
                    "This prevents conflicting registrations with any other modules.",
                    "NOTE: We may wish to enforce a policy where toolchain configuration is only allowed in the root module, or in rules_python. See https://github.com/bazelbuild/bazel/discussions/22024",
                )

            uv_repositories(
                uv_version = toolchain.uv_version,
                register_toolchains = False,
            )

uv = module_extension(
    doc = _DOC,
    implementation = _uv_toolchain_extension,
    tag_classes = {"toolchain": uv_toolchain},
)
