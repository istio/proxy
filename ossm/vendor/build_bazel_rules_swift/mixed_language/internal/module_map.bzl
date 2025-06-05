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

"""Implementation of the `mixed_language_module_map` macro."""

# buildifier: disable=bzl-visibility
load("//swift/internal:module_maps.bzl", "write_module_map")

def _mixed_language_internal_module_map_impl(ctx):
    actions = ctx.actions

    module_map = actions.declare_file(ctx.attr.module_map_name + ".modulemap")

    # TODO: Set `dependent_module_names`
    write_module_map(
        actions = actions,
        exported_module_ids = ["*"],
        module_map_file = module_map,
        module_name = ctx.attr.module_name,
        umbrella_header = ctx.file.umbrella_header,
        public_headers = ctx.files.hdrs,
        public_textual_headers = ctx.files.textual_hdrs,
    )

    return [DefaultInfo(files = depset([module_map]))]

mixed_language_internal_module_map = rule(
    attrs = {
        "hdrs": attr.label_list(
            allow_files = True,
            doc = """
The list of C, C++, Objective-C, and Objective-C++ header files published by
this library to be included by sources in dependent rules.
""",
        ),
        "module_map_name": attr.string(
            doc = "The name that will be used for the `.modulemap` file.",
            mandatory = True,
        ),
        "module_name": attr.string(
            doc = "The name of the module.",
            mandatory = True,
        ),
        "textual_hdrs": attr.label_list(
            allow_files = True,
            doc = """
The list of C, C++, Objective-C, and Objective-C++ files that are included as
headers by source files in this rule or by users of this library. Unlike `hdrs`,
these will not be compiled separately from the sources.
""",
        ),
        "umbrella_header": attr.label(
            allow_single_file = True,
            doc = """
An umbrella header. If set, `hdrs` and `textual_hdrs` are ignored.
""",
        ),
    },
    doc = """\
Creates the module map file that is used internally by the Clang and Swift
library targets in the `mixed_language_library` macro.
""",
    implementation = _mixed_language_internal_module_map_impl,
)
