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

_TEMPLATE = """\
load("@rules_python//python/uv/private:toolchains_hub.bzl", "toolchains_hub")

{}
"""

def _non_empty(d):
    return {k: v for k, v in d.items() if v}

def _toolchains_repo_impl(repository_ctx):
    contents = _TEMPLATE.format(
        render.call(
            "toolchains_hub",
            name = repr("uv_toolchain"),
            toolchains = render.list(repository_ctx.attr.toolchain_names),
            implementations = render.dict(
                repository_ctx.attr.toolchain_implementations,
            ),
            target_compatible_with = render.dict(
                repository_ctx.attr.toolchain_compatible_with,
                value_repr = render.list,
            ),
            target_settings = render.dict(
                _non_empty(repository_ctx.attr.toolchain_target_settings),
                value_repr = render.list,
            ),
        ),
    )
    repository_ctx.file("BUILD.bazel", contents)

uv_toolchains_repo = repository_rule(
    _toolchains_repo_impl,
    doc = "Generates a toolchain hub repository",
    attrs = {
        "toolchain_compatible_with": attr.string_list_dict(doc = "A list of platform constraints for this toolchain, keyed by toolchain name.", mandatory = True),
        "toolchain_implementations": attr.string_dict(doc = "The name of the toolchain implementation target, keyed by toolchain name.", mandatory = True),
        "toolchain_names": attr.string_list(doc = "List of toolchain names", mandatory = True),
        "toolchain_target_settings": attr.string_list_dict(doc = "A list of target_settings constraints for this toolchain, keyed by toolchain name.", mandatory = True),
        "toolchain_type": attr.string(doc = "The toolchain type of the toolchains", mandatory = True),
    },
)
