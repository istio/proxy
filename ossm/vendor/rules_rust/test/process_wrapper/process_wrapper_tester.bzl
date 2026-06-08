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

"""Process wrapper test."""

def _process_wrapper_tester_impl(ctx):
    args = ctx.actions.args()
    outputs = []
    combined = ctx.attr.test_config == "combined"

    if combined or ctx.attr.test_config == "stdout":
        stdout_output = ctx.actions.declare_file(ctx.label.name + ".stdout")
        outputs.append(stdout_output)
        args.add("--stdout-file", stdout_output)

    if combined or ctx.attr.test_config == "stderr":
        stderr_output = ctx.actions.declare_file(ctx.label.name + ".stderr")
        outputs.append(stderr_output)
        args.add("--stderr-file", stderr_output)

    if combined or (ctx.attr.test_config != "stdout" and ctx.attr.test_config != "stderr"):
        touch_output = ctx.actions.declare_file(ctx.label.name + ".touch")
        outputs.append(touch_output)
        args.add("--touch-file", touch_output)
        if ctx.attr.test_config == "copy-output":
            copy_output = ctx.actions.declare_file(ctx.label.name + ".touch.copy")
            outputs.append(copy_output)
            args.add_all("--copy-output", [touch_output, copy_output])

    if combined or ctx.attr.test_config == "env-files":
        args.add_all(ctx.files.env_files, before_each = "--env-file")

    if combined or ctx.attr.test_config == "arg-files":
        args.add_all(ctx.files.arg_files, before_each = "--arg-file")

    if combined or ctx.attr.test_config == "subst-pwd":
        args.add("--subst", "pwd=${pwd}")
        args.add("--subst", "key=value")

    args.add("--")

    args.add(ctx.executable._process_wrapper_tester)
    args.add(ctx.attr.test_config)
    args.add("--current-dir", "${pwd}")
    args.add("--test-subst", "subst key to ${key}")

    extra_args = ctx.actions.args()
    if combined or ctx.attr.test_config == "subst-pwd":
        extra_args.set_param_file_format("multiline")
        extra_args.use_param_file("@%s", use_always = True)
        extra_args.add("${pwd}")

    env = {"CURRENT_DIR": "${pwd}/test_path"}

    ctx.actions.run(
        mnemonic = "RustcProcessWrapperTester",
        executable = ctx.executable._process_wrapper,
        inputs = ctx.files.env_files + ctx.files.arg_files,
        outputs = outputs,
        arguments = [args, extra_args],
        env = env,
        tools = [ctx.executable._process_wrapper_tester],
    )

    return [DefaultInfo(files = depset(outputs))]

process_wrapper_tester = rule(
    implementation = _process_wrapper_tester_impl,
    doc = "This rule unit tests the different process_wrapper functionality.",
    attrs = {
        "arg_files": attr.label_list(
            doc = "Files containing newline delimited arguments.",
        ),
        "env_files": attr.label_list(
            doc = "Files containing newline delimited environment key/value pairs.",
        ),
        "test_config": attr.string(
            doc = "The desired test configuration.",
            mandatory = True,
            values = [
                "arg-files",
                "basic",
                "combined",
                "copy-output",
                "env-files",
                "stderr",
                "stdout",
                "subst-pwd",
            ],
        ),
        "use_param_file": attr.bool(
            doc = "Whether or not to use a params file with the process wrapper.",
            default = False,
        ),
        "_process_wrapper": attr.label(
            default = Label("//util/process_wrapper"),
            executable = True,
            allow_single_file = True,
            cfg = "exec",
        ),
        "_process_wrapper_tester": attr.label(
            default = Label("//test/process_wrapper:process_wrapper_tester"),
            executable = True,
            cfg = "exec",
        ),
    },
)
