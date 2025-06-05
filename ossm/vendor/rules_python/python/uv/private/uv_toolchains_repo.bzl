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

"Creates a repository to hold toolchains"

load("//python/private:text_util.bzl", "render")

_TOOLCHAIN_TEMPLATE = """
toolchain(
    name = "{name}",
    target_compatible_with = {compatible_with},
    toolchain = "{toolchain_label}",
    toolchain_type = "{toolchain_type}",
)
"""

def _toolchains_repo_impl(repository_ctx):
    build_content = ""
    for toolchain_name in repository_ctx.attr.toolchain_names:
        toolchain_label = repository_ctx.attr.toolchain_labels[toolchain_name]
        toolchain_compatible_with = repository_ctx.attr.toolchain_compatible_with[toolchain_name]

        build_content += _TOOLCHAIN_TEMPLATE.format(
            name = toolchain_name,
            toolchain_type = repository_ctx.attr.toolchain_type,
            toolchain_label = toolchain_label,
            compatible_with = render.list(toolchain_compatible_with),
        )

    repository_ctx.file("BUILD.bazel", build_content)

uv_toolchains_repo = repository_rule(
    _toolchains_repo_impl,
    doc = "Generates a toolchain hub repository",
    attrs = {
        "toolchain_compatible_with": attr.string_list_dict(doc = "A list of platform constraints for this toolchain, keyed by toolchain name.", mandatory = True),
        "toolchain_labels": attr.string_dict(doc = "The name of the toolchain implementation target, keyed by toolchain name.", mandatory = True),
        "toolchain_names": attr.string_list(doc = "List of toolchain names", mandatory = True),
        "toolchain_type": attr.string(doc = "The toolchain type of the toolchains", mandatory = True),
    },
)
