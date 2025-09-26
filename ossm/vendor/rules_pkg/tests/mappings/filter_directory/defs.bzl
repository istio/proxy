# Copyright 2021 The Bazel Authors. All rights reserved.
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

"""Rules and macros to support testing rules that output directories."""

load("@rules_python//python:defs.bzl", "py_test")

def _inspect_directory_script_impl(ctx):
    script = ctx.actions.declare_file("{}.py".format(ctx.attr.name))
    ctx.actions.expand_template(
        template = ctx.file._inspector_template,
        output = script,
        substitutions = {
            "%EXPECTED_STRUCTURE%": json.encode(ctx.attr.expected_structure),
            "%DIRECTORY_ROOT%": ctx.file.directory.short_path,
        },
    )

    return [DefaultInfo(files = depset([script]))]

_inspect_directory_script = rule(
    doc = """Create a script for testing the contents of directories.""",
    implementation = _inspect_directory_script_impl,
    attrs = {
        "directory": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
        "expected_structure": attr.string_list(
            mandatory = True,
        ),
        "_inspector_template": attr.label(
            default = ":inspect_directory.py.tpl",
            allow_single_file = True,
        ),
    },
)

def inspect_directory_test(name, directory, expected_structure, **kwargs):
    script_name = name + "_script"

    _inspect_directory_script(
        name = script_name,
        directory = directory,
        expected_structure = expected_structure,
    )

    # This appears to be necessary because of
    # https://github.com/bazelbuild/bazel/issues/1147
    native.sh_library(
        name = name + "_dir_lib",
        srcs = [directory],
    )

    py_test(
        name = name,
        srcs = [":" + script_name],
        main = ":" + script_name + ".py",
        data = [name + "_dir_lib"],
        deps = ["@rules_python//python/runfiles"],
        **kwargs
    )
