# Copyright 2020 The Bazel Authors. All rights reserved.
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
select_file() build rule implementation.

Selects a single file from the outputs of a target by given relative path.
"""

def _impl(ctx):
    if ctx.attr.subpath and len(ctx.attr.subpath) == 0:
        fail("Subpath can not be empty.")

    out = None
    canonical = ctx.attr.subpath.replace("\\", "/")
    for file_ in ctx.attr.srcs.files.to_list():
        if file_.path.replace("\\", "/").endswith(canonical):
            out = file_
            break
    if not out:
        files_str = ",\n".join([
            str(f.path)
            for f in ctx.attr.srcs.files.to_list()
        ])
        fail("Can not find specified file in [%s]" % files_str)
    return [DefaultInfo(files = depset([out]))]

select_file = rule(
    implementation = _impl,
    doc = "Selects a single file from the outputs of a target by given relative path",
    attrs = {
        "srcs": attr.label(
            allow_files = True,
            mandatory = True,
            doc = "The target producing the file among other outputs",
        ),
        "subpath": attr.string(
            mandatory = True,
            doc = "Relative path to the file",
        ),
    },
)
