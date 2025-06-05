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

"""
A private rule to generate an entry_point python file to be used in a py_binary.

Right now it only supports console_scripts via the entry_points.txt file in the dist-info.

NOTE @aignas 2023-08-07: This cannot be in pure starlark, because we need to
read a file and then create a `.py` file based on the contents of that file,
which cannot be done in pure starlark according to
https://github.com/bazelbuild/bazel/issues/14744
"""

_ENTRY_POINTS_TXT = "entry_points.txt"

def _get_entry_points_txt(entry_points_txt):
    """Get the entry_points.txt file

    TODO: use map_each to avoid flattening of the directories outside the execution phase.
    """
    for file in entry_points_txt.files.to_list():
        if file.basename == _ENTRY_POINTS_TXT:
            return file

    fail("{} does not contain {}".format(entry_points_txt, _ENTRY_POINTS_TXT))

def _py_console_script_gen_impl(ctx):
    entry_points_txt = _get_entry_points_txt(ctx.attr.entry_points_txt)

    args = ctx.actions.args()
    args.add("--console-script", ctx.attr.console_script)
    args.add("--console-script-guess", ctx.attr.console_script_guess)
    args.add(entry_points_txt)
    args.add(ctx.outputs.out)

    ctx.actions.run(
        inputs = [
            entry_points_txt,
        ],
        outputs = [ctx.outputs.out],
        arguments = [args],
        mnemonic = "PyConsoleScriptBinaryGen",
        progress_message = "Generating py_console_script_binary main: %{label}",
        executable = ctx.executable._tool,
    )

    return [DefaultInfo(
        files = depset([ctx.outputs.out]),
    )]

py_console_script_gen = rule(
    _py_console_script_gen_impl,
    attrs = {
        "console_script": attr.string(
            doc = "The name of the console_script to create the .py file for. Optional if there is only a single entry-point available.",
            default = "",
            mandatory = False,
        ),
        "console_script_guess": attr.string(
            doc = "The string used for guessing the console_script if it is not provided.",
            default = "",
            mandatory = False,
        ),
        "entry_points_txt": attr.label(
            doc = "The filegroup to search for entry_points.txt.",
            mandatory = True,
        ),
        "out": attr.output(
            doc = "Output file location.",
            mandatory = True,
        ),
        "_tool": attr.label(
            default = ":py_console_script_gen_py",
            executable = True,
            cfg = "exec",
        ),
    },
    doc = """\
Builds an entry_point script from an entry_points.txt file.
""",
)
