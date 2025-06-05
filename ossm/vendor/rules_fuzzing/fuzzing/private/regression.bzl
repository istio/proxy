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

"""Regression testing rule for fuzz tests."""

load("//fuzzing/private:binary.bzl", "FuzzingBinaryInfo")

def _fuzzing_regression_test_impl(ctx):
    binary_info = ctx.attr.binary[FuzzingBinaryInfo]
    script = ctx.actions.declare_file(ctx.label.name)
    script_template = """
export FUZZER_OUTPUT_CORPUS_DIR="$TEST_TMPDIR/corpus"
export FUZZER_ARTIFACTS_DIR="$TEST_TMPDIR/artifacts"
export FUZZER_BINARY='{fuzzer_binary}'
export FUZZER_SEED_CORPUS_DIR='{seed_corpus_dir}'
export FUZZER_IS_REGRESSION=1
{engine_launcher_environment}

mkdir -p "$FUZZER_OUTPUT_CORPUS_DIR"
mkdir -p "$FUZZER_ARTIFACTS_DIR"

exec '{engine_launcher}'
"""
    script_content = script_template.format(
        fuzzer_binary = ctx.executable.binary.short_path,
        seed_corpus_dir = binary_info.corpus_dir.short_path,
        engine_launcher_environment = "\n".join([
            "export %s='%s'" % (var, file.short_path)
            for var, file in binary_info.engine_info.launcher_environment.items()
        ]),
        engine_launcher = binary_info.engine_info.launcher.short_path,
    )
    ctx.actions.write(script, script_content, is_executable = True)

    runfiles = ctx.runfiles()
    runfiles = runfiles.merge(ctx.attr.binary[DefaultInfo].default_runfiles)
    runfiles = runfiles.merge(binary_info.engine_info.launcher_runfiles)

    return [
        DefaultInfo(executable = script, runfiles = runfiles),
        coverage_common.instrumented_files_info(
            ctx,
            dependency_attributes = ["binary"],
        ),
    ]

fuzzing_regression_test = rule(
    implementation = _fuzzing_regression_test_impl,
    doc = """
Executes a fuzz test on its seed corpus.
""",
    attrs = {
        "binary": attr.label(
            executable = True,
            doc = "The instrumented executable of the fuzz test to run.",
            providers = [FuzzingBinaryInfo],
            cfg = "target",
            mandatory = True,
        ),
        "_lcov_merger": attr.label(
            # As of Bazel 5.1.0, the following would work instead of the alias used below:
            # default = configuration_field(fragment = "coverage", name = "output_generator")
            default = "//fuzzing/tools:lcov_merger",
            executable = True,
            # This needs to be built in the target configuration so that the alias it points to can
            # select on the value of --collect_code_coverage, which is disabled in the exec
            # configuration. Since target and exec platform usually coincide for test execution,
            # this should not cause any problems.
            cfg = "target",
        ),
        "_collect_cc_coverage": attr.label(
            # This target is just a shell script and can thus be depended on unconditionally
            # without any effect on build times.
            default = "@bazel_tools//tools/test:collect_cc_coverage",
            executable = True,
            cfg = "target",
        ),
    },
    test = True,
)
