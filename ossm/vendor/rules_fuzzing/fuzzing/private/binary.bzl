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

"""Defines a rule for creating an instrumented fuzzing executable."""

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load(
    "//fuzzing:instrum_opts.bzl",
    "instrum_configs",
    "sanitizer_configs",
)
load("//fuzzing/private:engine.bzl", "FuzzingEngineInfo")
load(
    "//fuzzing/private:instrum_opts.bzl",
    "instrum_defaults",
    "instrum_opts",
)

FuzzingBinaryInfo = provider(
    doc = """
Provider for storing information about a fuzz test binary.
""",
    fields = {
        "binary_file": "The instrumented fuzz test executable.",
        "binary_runfiles": "The runfiles of the fuzz test executable.",
        "binary_repo_mapping_manifest": "The _repo_mapping file of the fuzz " +
                                        "test executable.",
        "corpus_dir": "The directory of the corpus files used as input seeds.",
        "dictionary_file": "The dictionary file to use in fuzzing runs.",
        "engine_info": "The `FuzzingEngineInfo` provider of the fuzzing " +
                       "engine used in the fuzz test.",
        "options_file": "A file containing fuzzing engine and sanitizer " +
                        "options to use during execution. The file loosely " +
                        "follows the INI format and currently only applies " +
                        "to OSS-Fuzz.",
    },
)

def _fuzzing_binary_transition_impl(settings, _attr):
    opts = instrum_opts.make(
        copts = settings["//command_line_option:copt"],
        conlyopts = settings["//command_line_option:conlyopt"],
        cxxopts = settings["//command_line_option:cxxopt"],
        linkopts = settings["//command_line_option:linkopt"],
    )

    is_fuzzing_build_mode = settings["@rules_fuzzing//fuzzing:cc_fuzzing_build_mode"]
    if is_fuzzing_build_mode:
        opts = instrum_opts.merge(opts, instrum_defaults.fuzzing_build)

    instrum_config = settings["@rules_fuzzing//fuzzing:cc_engine_instrumentation"]
    if instrum_config in instrum_configs:
        opts = instrum_opts.merge(opts, instrum_configs[instrum_config])
    else:
        fail("unsupported engine instrumentation '%s'" % instrum_config)

    sanitizer_config = settings["@rules_fuzzing//fuzzing:cc_engine_sanitizer"]
    if sanitizer_config in sanitizer_configs:
        opts = instrum_opts.merge(opts, sanitizer_configs[sanitizer_config])
    else:
        fail("unsupported sanitizer '%s'" % sanitizer_config)

    return {
        "//command_line_option:copt": opts.copts,
        "//command_line_option:linkopt": opts.linkopts,
        "//command_line_option:conlyopt": opts.conlyopts,
        "//command_line_option:cxxopt": opts.cxxopts,
        # Make sure binaries are built statically, to maximize the scope of the
        # instrumentation.
        "//command_line_option:dynamic_mode": "off",
    }

fuzzing_binary_transition = transition(
    implementation = _fuzzing_binary_transition_impl,
    inputs = [
        "@rules_fuzzing//fuzzing:cc_engine_instrumentation",
        "@rules_fuzzing//fuzzing:cc_engine_sanitizer",
        "@rules_fuzzing//fuzzing:cc_fuzzing_build_mode",
        "//command_line_option:copt",
        "//command_line_option:conlyopt",
        "//command_line_option:cxxopt",
        "//command_line_option:linkopt",
    ],
    outputs = [
        "//command_line_option:copt",
        "//command_line_option:conlyopt",
        "//command_line_option:cxxopt",
        "//command_line_option:linkopt",
        "//command_line_option:dynamic_mode",
    ],
)

def _fuzzing_binary_impl(ctx):
    output_file = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.symlink(
        output = output_file,
        target_file = ctx.executable.binary,
        is_executable = True,
    )
    if ctx.attr._instrument_binary:
        # The attribute is a list if a transition is attached.
        default_info = ctx.attr.binary[0][DefaultInfo]
    else:
        default_info = ctx.attr.binary[DefaultInfo]
    binary_runfiles = default_info.default_runfiles
    binary_repo_mapping_manifest = getattr(default_info.files_to_run, "repo_mapping_manifest", None)
    other_runfiles = []
    if ctx.file.corpus:
        other_runfiles.append(ctx.file.corpus)
    if ctx.file.dictionary:
        other_runfiles.append(ctx.file.dictionary)
    if ctx.file.options:
        other_runfiles.append(ctx.file.options)
    return [
        DefaultInfo(
            executable = output_file,
            runfiles = binary_runfiles.merge(ctx.runfiles(files = other_runfiles)),
        ),
        FuzzingBinaryInfo(
            binary_file = ctx.executable.binary,
            binary_runfiles = binary_runfiles,
            binary_repo_mapping_manifest = binary_repo_mapping_manifest,
            corpus_dir = ctx.file.corpus,
            dictionary_file = ctx.file.dictionary,
            engine_info = ctx.attr.engine[FuzzingEngineInfo],
            options_file = ctx.file.options,
        ),
        coverage_common.instrumented_files_info(
            ctx,
            dependency_attributes = ["binary"],
        ),
    ]

_common_fuzzing_binary_attrs = {
    "engine": attr.label(
        doc = "The specification of the fuzzing engine used in the binary.",
        providers = [FuzzingEngineInfo],
        mandatory = True,
    ),
    "corpus": attr.label(
        doc = "A directory of corpus files used as input seeds.",
        allow_single_file = True,
    ),
    "dictionary": attr.label(
        doc = "A dictionary file to use in fuzzing runs.",
        allow_single_file = True,
    ),
    "options": attr.label(
        doc = "A file containing fuzzing engine and sanitizer options to use " +
              "use during execution. The file loosely follows the INI " +
              "format and currently only applies to OSS-Fuzz.",
        allow_single_file = True,
    ),
}

fuzzing_binary = rule(
    implementation = _fuzzing_binary_impl,
    doc = """
Creates an instrumented fuzzing executable.

The executable runfiles include the corpus directory and the dictionary file,
if specified.

The instrumentation is controlled by the following flags:

 * `@rules_fuzzing//fuzzing:cc_engine_instrumentation`
 * `@rules_fuzzing//fuzzing:cc_engine_sanitizer`
 * `@rules_fuzzing//fuzzing:cc_fuzzing_build_mode`
""",
    attrs = dicts.add(_common_fuzzing_binary_attrs, {
        "binary": attr.label(
            executable = True,
            doc = "The fuzz test executable to instrument.",
            cfg = fuzzing_binary_transition,
            mandatory = True,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
        "_instrument_binary": attr.bool(
            default = True,
        ),
    }),
    executable = True,
    provides = [FuzzingBinaryInfo],
)

fuzzing_binary_uninstrumented = rule(
    implementation = _fuzzing_binary_impl,
    doc = """
Creates an uninstrumented fuzzing executable.

The fuzz test still requires instrumentation to function correctly, so it should
be incorporated in the target configuration (e.g., on the command line or the
.bazelrc configuration file).
""",
    attrs = dicts.add(_common_fuzzing_binary_attrs, {
        "binary": attr.label(
            executable = True,
            doc = "The instrumented fuzz test executable.",
            cfg = "target",
            mandatory = True,
        ),
        "_instrument_binary": attr.bool(
            default = False,
        ),
    }),
    executable = True,
    provides = [FuzzingBinaryInfo],
)
