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

"""The implementation of the {cc, java}_fuzzing_engine rules."""

load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load("@rules_java//java/common:java_info.bzl", "JavaInfo")

FuzzingEngineInfo = provider(
    doc = """
Provider for storing the language-independent part of the specification of a fuzzing engine.
""",
    fields = {
        "display_name": "A string representing the human-readable name of the fuzzing engine.",
        "launcher": "A file representing the shell script that launches the fuzz target.",
        "launcher_runfiles": "The runfiles needed by the launcher script on the fuzzing engine side, such as helper tools and their data dependencies.",
        "launcher_environment": "A dictionary from environment variables to files used by the launcher script.",
    },
)

def _make_fuzzing_engine_info(ctx):
    if not ctx.attr.display_name:
        fail("The display_name attribute of the rule must not be empty.")

    launcher_runfiles = ctx.runfiles(files = [ctx.file.launcher])
    env_vars = {}
    for data_label, data_env_var in ctx.attr.launcher_data.items():
        data_files = data_label.files.to_list()
        if data_env_var:
            if data_env_var in env_vars:
                fail("Multiple data dependencies map to variable '%s'." % data_env_var)
            if len(data_files) != 1:
                fail("Data dependency for variable '%s' doesn't map to exactly one file." % data_env_var)
            env_vars[data_env_var] = data_files[0]
        launcher_runfiles = launcher_runfiles.merge(ctx.runfiles(files = data_files))
        launcher_runfiles = launcher_runfiles.merge(data_label[DefaultInfo].default_runfiles)

    return FuzzingEngineInfo(
        display_name = ctx.attr.display_name,
        launcher = ctx.file.launcher,
        launcher_runfiles = launcher_runfiles,
        launcher_environment = env_vars,
    )

def _cc_fuzzing_engine_impl(ctx):
    return [
        _make_fuzzing_engine_info(ctx),
        ctx.attr.library[DefaultInfo],
        ctx.attr.library[CcInfo],
    ]

cc_fuzzing_engine = rule(
    implementation = _cc_fuzzing_engine_impl,
    doc = """
Specifies a fuzzing engine that can be used to run C++ fuzz targets.
""",
    attrs = {
        "display_name": attr.string(
            doc = "The name of the fuzzing engine, as it should be rendered " +
                  "in human-readable output.",
            mandatory = True,
        ),
        "library": attr.label(
            doc = "A cc_library target that implements the fuzzing engine " +
                  "entry point.",
            mandatory = True,
            providers = [CcInfo],
        ),
        "launcher": attr.label(
            doc = "A shell script that knows how to launch the fuzzing " +
                  "executable based on configuration specified in the environment.",
            mandatory = True,
            allow_single_file = True,
        ),
        "launcher_data": attr.label_keyed_string_dict(
            doc = "A dict mapping additional runtime dependencies needed by " +
                  "the fuzzing engine to environment variables that will be " +
                  "available inside the launcher, holding the runtime path " +
                  "to the dependency.",
            allow_files = True,
        ),
    },
    provides = [CcInfo, FuzzingEngineInfo],
)

def _java_fuzzing_engine_impl(ctx):
    return [
        _make_fuzzing_engine_info(ctx),
        ctx.attr.library[DefaultInfo],
        ctx.attr.library[JavaInfo],
    ]

java_fuzzing_engine = rule(
    implementation = _java_fuzzing_engine_impl,
    doc = """
Specifies a fuzzing engine that can be used to run Java fuzz targets.
""",
    attrs = {
        "display_name": attr.string(
            doc = "The name of the fuzzing engine, as it should be rendered " +
                  "in human-readable output.",
            mandatory = True,
        ),
        "library": attr.label(
            doc = "A java_library target that is made available to all Java " +
                  "fuzz tests.",
            providers = [JavaInfo],
        ),
        "launcher": attr.label(
            doc = "A shell script that knows how to launch the fuzzing " +
                  "executable based on configuration specified in the environment.",
            mandatory = True,
            allow_single_file = True,
        ),
        "launcher_data": attr.label_keyed_string_dict(
            doc = "A dict mapping additional runtime dependencies needed by " +
                  "the fuzzing engine to environment variables that will be " +
                  "available inside the launcher, holding the runtime path " +
                  "to the dependency.",
            allow_files = True,
        ),
    },
    provides = [FuzzingEngineInfo, JavaInfo],
)
