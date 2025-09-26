# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Miscellaneous utilities."""

load("@bazel_skylib//lib:paths.bzl", "paths")

def _generate_file_impl(ctx):
    ctx.actions.write(ctx.outputs.output, ctx.attr.contents)

generate_file = rule(
    implementation = _generate_file_impl,
    doc = """
Generates a file with a specified content string.
""",
    attrs = {
        "contents": attr.string(
            doc = "The file contents.",
            mandatory = True,
        ),
        "output": attr.output(
            doc = "The output file to write.",
            mandatory = True,
        ),
    },
)

# Returns the path of a runfile that can be used to look up its absolute path
# via the rlocation function provided by Bazel's runfiles libraries.
def runfile_path(ctx, runfile):
    return paths.normalize(ctx.workspace_name + "/" + runfile.short_path)
