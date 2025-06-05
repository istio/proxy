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

"""Implementation of the `mixed_language_umbrella_header` rule."""

load("@bazel_skylib//lib:paths.bzl", "paths")

def _mixed_language_umbrella_header_impl(ctx):
    actions = ctx.actions

    umbrella_header = actions.declare_file(
        "{name}/{module_name}/{module_name}.h".format(
            module_name = ctx.attr.module_name,
            name = ctx.attr.name,
        ),
    )

    content = actions.args()
    content.set_param_file_format("multiline")
    content.add_all(ctx.files.hdrs, format_each = '#import "%s"')
    actions.write(
        content = content,
        output = umbrella_header,
    )
    outputs = [umbrella_header]
    outputs_depset = depset(outputs)

    compilation_context = cc_common.create_compilation_context(
        headers = outputs_depset,
        direct_public_headers = outputs,
        # This allows `#import <module_name/module_name.h>` to work
        includes = depset([paths.dirname(umbrella_header.dirname)]),
    )

    return [
        DefaultInfo(files = outputs_depset),
        CcInfo(compilation_context = compilation_context),
    ]

mixed_language_umbrella_header = rule(
    attrs = {
        "hdrs": attr.label_list(
            allow_files = True,
            doc = """
The list of C, C++, Objective-C, and Objective-C++ header files published by
this library to be included by sources in dependent rules.
""",
        ),
        "module_name": attr.string(
            doc = "The name of the module.",
            mandatory = True,
        ),
    },
    doc = "Creates an umbrella header for a mixed language target.",
    implementation = _mixed_language_umbrella_header_impl,
)
