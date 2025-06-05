# Copyright 2023 The Bazel Authors. All rights reserved.
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

"""Public entry point for py_import rule."""

load(":py_info.bzl", "PyInfo")

def _py_import_impl(ctx):
    # See https://github.com/bazelbuild/bazel/blob/0.24.0/src/main/java/com/google/devtools/build/lib/bazel/rules/python/BazelPythonSemantics.java#L104 .
    import_paths = [
        "/".join([ctx.workspace_name, x.short_path])
        for x in ctx.files.srcs
    ]

    return [
        DefaultInfo(
            default_runfiles = ctx.runfiles(ctx.files.srcs, collect_default = True),
        ),
        PyInfo(
            transitive_sources = depset(transitive = [
                dep[PyInfo].transitive_sources
                for dep in ctx.attr.deps
            ]),
            imports = depset(direct = import_paths, transitive = [
                dep[PyInfo].imports
                for dep in ctx.attr.deps
            ]),
        ),
    ]

py_import = rule(
    doc = """This rule allows the use of Python packages as dependencies.

    It imports the given `.egg` file(s), which might be checked in source files,
    fetched externally as with `http_file`, or produced as outputs of other rules.

    It may be used like a `py_library`, in the `deps` of other Python rules.

    This is similar to [java_import](https://docs.bazel.build/versions/master/be/java.html#java_import).
    """,
    implementation = _py_import_impl,
    attrs = {
        "deps": attr.label_list(
            doc = "The list of other libraries to be linked in to the " +
                  "binary target.",
            providers = [PyInfo],
        ),
        "srcs": attr.label_list(
            doc = "The list of Python package files provided to Python targets " +
                  "that depend on this target. Note that currently only the .egg " +
                  "format is accepted. For .whl files, try the whl_library rule. " +
                  "We accept contributions to extend py_import to handle .whl.",
            allow_files = [".egg"],
        ),
    },
)
