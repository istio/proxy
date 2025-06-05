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

"""Common building blocks for fuzz test definitions."""

load("//fuzzing/private:binary.bzl", "FuzzingBinaryInfo")

def _fuzzing_launcher_script(ctx):
    binary_info = ctx.attr.binary[FuzzingBinaryInfo]
    script = ctx.actions.declare_file(ctx.label.name)

    script_template = """
{environment}
echo "Launching {binary_path} as a {engine_name} fuzz test..."
RUNFILES_DIR="$0.runfiles" \
exec "{launcher}" \
    --engine_launcher="{engine_launcher}" \
    --binary_path="{binary_path}" \
    --corpus_dir="{corpus_dir}" \
    --dictionary_path="{dictionary_path}" \
    "$@"
"""
    script_content = script_template.format(
        environment = "\n".join([
            "export %s='%s'" % (var, file.short_path)
            for var, file in binary_info.engine_info.launcher_environment.items()
        ]),
        launcher = ctx.executable._launcher.short_path,
        binary_path = ctx.executable.binary.short_path,
        engine_launcher = binary_info.engine_info.launcher.short_path,
        engine_name = binary_info.engine_info.display_name,
        corpus_dir = binary_info.corpus_dir.short_path if binary_info.corpus_dir else "",
        dictionary_path = binary_info.dictionary_file.short_path if binary_info.dictionary_file else "",
    )
    ctx.actions.write(script, script_content, is_executable = True)
    return script

def _fuzzing_launcher_impl(ctx):
    script = _fuzzing_launcher_script(ctx)

    binary_info = ctx.attr.binary[FuzzingBinaryInfo]
    runfiles = ctx.runfiles()
    runfiles = runfiles.merge(binary_info.engine_info.launcher_runfiles)
    runfiles = runfiles.merge(ctx.attr._launcher[DefaultInfo].default_runfiles)
    runfiles = runfiles.merge(ctx.attr.binary[DefaultInfo].default_runfiles)
    return [DefaultInfo(executable = script, runfiles = runfiles)]

fuzzing_launcher = rule(
    implementation = _fuzzing_launcher_impl,
    doc = """
Rule for creating a script to run the fuzzing test.
""",
    attrs = {
        "_launcher": attr.label(
            default = Label("//fuzzing/tools:launcher"),
            doc = "The launcher script to start the fuzzing test.",
            executable = True,
            cfg = "exec",
        ),
        "binary": attr.label(
            executable = True,
            doc = "The executable of the fuzz test to run.",
            providers = [FuzzingBinaryInfo],
            cfg = "target",
            mandatory = True,
        ),
    },
    executable = True,
)

def _fuzzing_corpus_impl(ctx):
    corpus_list_file_args = ctx.actions.args()
    corpus_list_file_args.set_param_file_format("multiline")
    corpus_list_file_args.use_param_file("--corpus_list_file=%s", use_always = True)
    corpus_list_file_args.add_all(ctx.files.srcs)

    corpus_dir = ctx.actions.declare_directory(ctx.attr.name)
    cp_args = ctx.actions.args()
    cp_args.add("--output_dir=" + corpus_dir.path)

    ctx.actions.run(
        inputs = ctx.files.srcs,
        outputs = [corpus_dir],
        arguments = [cp_args, corpus_list_file_args],
        executable = ctx.executable._corpus_tool,
        # Use the default rather than an empty environment so that PATH is
        # set and python can be found.
        use_default_shell_env = True,
    )

    return [DefaultInfo(
        runfiles = ctx.runfiles(files = [corpus_dir]),
        files = depset([corpus_dir]),
    )]

fuzzing_corpus = rule(
    implementation = _fuzzing_corpus_impl,
    doc = """
This rule provides a <name>_corpus directory collecting all the corpora files 
specified in the srcs attribute.
""",
    attrs = {
        "_corpus_tool": attr.label(
            default = Label("//fuzzing/tools:make_corpus_dir"),
            doc = "The tool script to copy and rename the corpus.",
            executable = True,
            cfg = "exec",
        ),
        "srcs": attr.label_list(
            doc = "The corpus files for the fuzzing test.",
            allow_files = True,
        ),
    },
)

def _fuzzing_dictionary_impl(ctx):
    output_dict = ctx.actions.declare_file(ctx.attr.output)
    args = ctx.actions.args()
    args.add_joined("--dict_list", ctx.files.dicts, join_with = ",")
    args.add("--output_file=" + output_dict.path)

    ctx.actions.run(
        inputs = ctx.files.dicts,
        outputs = [output_dict],
        arguments = [args],
        executable = ctx.executable._validation_tool,
        # Use the default rather than an empty environment so that PATH is
        # set and python can be found.
        use_default_shell_env = True,
    )

    runfiles = ctx.runfiles(files = [output_dict])

    return [DefaultInfo(
        runfiles = runfiles,
        files = depset([output_dict]),
    )]

fuzzing_dictionary = rule(
    implementation = _fuzzing_dictionary_impl,
    doc = """
Rule to validate the fuzzing dictionaries and output a merged dictionary.
""",
    attrs = {
        "_validation_tool": attr.label(
            default = Label("//fuzzing/tools:validate_dict"),
            doc = "The tool script to validate and merge the dictionaries.",
            executable = True,
            cfg = "exec",
        ),
        "dicts": attr.label_list(
            doc = "The fuzzing dictionaries.",
            allow_files = True,
            mandatory = True,
        ),
        "output": attr.string(
            doc = "The name of the merged dictionary.",
            mandatory = True,
        ),
    },
)
