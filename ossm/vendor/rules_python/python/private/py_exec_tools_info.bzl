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
"""Implementation of the exec tools toolchain provider."""

PyExecToolsInfo = provider(
    doc = "Build tools used as part of building Python programs.",
    fields = {
        "exec_interpreter": """
:type: Target | None

If available, an interpreter valid for running in the exec configuration.
When running it in an action, use `DefaultInfo.files_to_run` to ensure all its
files are appropriately available. An exec interpreter may not be available,
e.g. if all the exec tools are prebuilt binaries.

NOTE: this interpreter is really only for use when a build tool cannot use
the Python toolchain itself. When possible, prefeer to define a `py_binary`
instead and use it via a `cfg=exec` attribute; this makes it much easier
to setup the runtime environment for the binary. See also:
`py_interpreter_program` rule.

NOTE: What interpreter is used depends on the toolchain constraints. Ensure
the proper target constraints are being applied when obtaining this from
the toolchain.
""",
        "precompiler": """
:type: Target | None

If available, the tool to use for generating pyc files. If not available,
precompiling will not be available.

Must provide one of the following:
  * PyInterpreterProgramInfo
  * DefaultInfo.files_to_run

This target provides either the `PyInterpreterProgramInfo` provider or is a
regular executable binary (provides DefaultInfo.files_to_run). When the
`PyInterpreterProgramInfo` provider is present, it means the precompiler program
doesn't know how to find the interpreter itself, so the caller must provide it
when constructing the action invocation for running the precompiler program
(typically `exec_interpreter`). See the `PyInterpreterProgramInfo` provider docs
for details on how to construct an invocation.

If {obj}`testing.ExecutionInfo` is provided, it will be used to set execution
requirements. This can be used to control persistent worker settings.

The precompiler command line API is:
* `--invalidation_mode`: The type of pyc invalidation mode to use. Should be
  one of `unchecked_hash` or `checked_hash`.
* `--optimize`: The optimization level as an integer.
* `--python_version`: The Python version, in `Major.Minor` format, e.g. `3.12`

The following args are repeated and form a list of 3-tuples of their values. At
least one 3-tuple will be passed.
* `--src`: Path to the source `.py` file to precompile.
* `--src_name`: The human-friendly file name to record in the pyc output.
* `--pyc`: Path to where pyc output should be written.

NOTE: These arguments _may_ be stored in a file instead, in which case, the
path to that file will be a positional arg starting with `@`, e.g. `@foo/bar`.
The format of the file is one arg per line.
""",
    },
)
