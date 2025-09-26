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

"""Definitions for the modules_mapping.json generation.

The modules_mapping.json file is a mapping from Python modules to the wheel
names that provide those modules. It is used for determining which wheel
distribution should be used in the `deps` attribute of `py_*` targets.

This mapping is necessary when reading Python import statements and determining
if they are provided by third-party dependencies. Most importantly, when the
module name doesn't match the wheel distribution name.
"""

def _modules_mapping_impl(ctx):
    modules_mapping = ctx.actions.declare_file(ctx.attr.modules_mapping_name)
    all_wheels = depset(
        [whl for whl in ctx.files.wheels],
        transitive = [dep[DefaultInfo].files for dep in ctx.attr.wheels] + [dep[DefaultInfo].data_runfiles.files for dep in ctx.attr.wheels],
    )

    args = ctx.actions.args()

    # Spill parameters to a file prefixed with '@'. Note, the '@' prefix is the same
    # prefix as used in the `generator.py` in `fromfile_prefix_chars` attribute.
    args.use_param_file(param_file_arg = "@%s")
    args.set_param_file_format(format = "multiline")
    if ctx.attr.include_stub_packages:
        args.add("--include_stub_packages")
    args.add("--output_file", modules_mapping)
    args.add_all("--exclude_patterns", ctx.attr.exclude_patterns)
    args.add_all("--wheels", all_wheels)

    ctx.actions.run(
        inputs = all_wheels,
        outputs = [modules_mapping],
        executable = ctx.executable._generator,
        arguments = [args],
        use_default_shell_env = False,
    )
    return [DefaultInfo(files = depset([modules_mapping]))]

modules_mapping = rule(
    _modules_mapping_impl,
    attrs = {
        "exclude_patterns": attr.string_list(
            default = ["^_|(\\._)+"],
            doc = "A set of regex patterns to match against each calculated module path. By default, exclude the modules starting with underscores.",
            mandatory = False,
        ),
        "include_stub_packages": attr.bool(
            default = False,
            doc = "Whether to include stub packages in the mapping.",
            mandatory = False,
        ),
        "modules_mapping_name": attr.string(
            default = "modules_mapping.json",
            doc = "The name for the output JSON file.",
            mandatory = False,
        ),
        "wheels": attr.label_list(
            allow_files = True,
            doc = "The list of wheels, usually the 'all_whl_requirements' from @<pip_repository>//:requirements.bzl",
            mandatory = True,
        ),
        "_generator": attr.label(
            cfg = "exec",
            default = "//modules_mapping:generator",
            executable = True,
        ),
    },
    doc = "Creates a modules_mapping.json file for mapping module names to wheel distribution names.",
)
