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

"""group_library implementation for WORKSPACE setups."""

load(":generate_group_library_build_bazel.bzl", "generate_group_library_build_bazel")

def _group_library_impl(rctx):
    build_file_contents = generate_group_library_build_bazel(
        repo_prefix = rctx.attr.repo_prefix,
        groups = rctx.attr.groups,
    )
    rctx.file("BUILD.bazel", build_file_contents)

group_library = repository_rule(
    attrs = {
        "groups": attr.string_list_dict(
            doc = "A mapping of group names to requirements within that group.",
        ),
        "repo_prefix": attr.string(
            doc = "Prefix used for the whl_library created components of each group",
        ),
    },
    implementation = _group_library_impl,
    doc = """
Create a package containing only wrapper py_library and whl_library rules for implementing dependency groups.
This is an implementation detail of dependency groups and should not be used alone.
    """,
)
