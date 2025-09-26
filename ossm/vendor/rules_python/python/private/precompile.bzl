# Copyright 2022 The Bazel Authors. All rights reserved.
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
"""Common functions that are specific to Bazel rule implementation"""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load(":attributes.bzl", "PrecompileAttr", "PrecompileInvalidationModeAttr", "PrecompileSourceRetentionAttr")
load(":flags.bzl", "PrecompileFlag")
load(":py_interpreter_program.bzl", "PyInterpreterProgramInfo")
load(":toolchain_types.bzl", "EXEC_TOOLS_TOOLCHAIN_TYPE", "TARGET_TOOLCHAIN_TYPE")

def maybe_precompile(ctx, srcs):
    """Computes all the outputs (maybe precompiled) from the input srcs.

    See create_binary_semantics_struct for details about this function.

    Args:
        ctx: Rule ctx.
        srcs: List of Files; the inputs to maybe precompile.

    Returns:
        Struct of precompiling results with fields:
        * `keep_srcs`: list of File; the input sources that should be included
          as default outputs.
        * `pyc_files`: list of File; the precompiled files.
        * `py_to_pyc_map`: dict of src File input to pyc File output. If a source
          file wasn't precompiled, it won't be in the dict.
    """

    # The exec tools toolchain and precompiler are optional. Rather than
    # fail, just skip precompiling, as its mostly just an optimization.
    exec_tools_toolchain = ctx.toolchains[EXEC_TOOLS_TOOLCHAIN_TYPE]
    if exec_tools_toolchain == None or exec_tools_toolchain.exec_tools.precompiler == None:
        precompile = PrecompileAttr.DISABLED
    else:
        precompile_flag = ctx.attr._precompile_flag[BuildSettingInfo].value

        if precompile_flag == PrecompileFlag.FORCE_ENABLED:
            precompile = PrecompileAttr.ENABLED
        elif precompile_flag == PrecompileFlag.FORCE_DISABLED:
            precompile = PrecompileAttr.DISABLED
        else:
            precompile = ctx.attr.precompile

    # Unless explicitly disabled, we always generate a pyc. This allows
    # binaries to decide whether to include them or not later.
    if precompile != PrecompileAttr.DISABLED:
        should_precompile = True
    else:
        should_precompile = False

    source_retention = PrecompileSourceRetentionAttr.get_effective_value(ctx)
    keep_source = (
        not should_precompile or
        source_retention == PrecompileSourceRetentionAttr.KEEP_SOURCE
    )

    result = struct(
        keep_srcs = [],
        pyc_files = [],
        py_to_pyc_map = {},
    )
    for src in srcs:
        if should_precompile:
            # NOTE: _precompile() may return None
            pyc = _precompile(ctx, src, use_pycache = keep_source)
        else:
            pyc = None

        if pyc:
            result.pyc_files.append(pyc)
            result.py_to_pyc_map[src] = pyc

        if keep_source or not pyc:
            result.keep_srcs.append(src)

    return result

def _precompile(ctx, src, *, use_pycache):
    """Compile a py file to pyc.

    Args:
        ctx: rule context.
        src: File object to compile
        use_pycache: bool. True if the output should be within the `__pycache__`
            sub-directory. False if it should be alongside the original source
            file.

    Returns:
        File of the generated pyc file.
    """

    # Generating a file in another package is an error, so we have to skip
    # such cases.
    if ctx.label.package != src.owner.package:
        return None

    exec_tools_info = ctx.toolchains[EXEC_TOOLS_TOOLCHAIN_TYPE].exec_tools
    target_toolchain = ctx.toolchains[TARGET_TOOLCHAIN_TYPE].py3_runtime

    # These args control starting the precompiler, e.g., when run as a worker,
    # these args are only passed once.
    precompiler_startup_args = ctx.actions.args()

    env = {}
    tools = []

    precompiler = exec_tools_info.precompiler
    if PyInterpreterProgramInfo in precompiler:
        precompiler_executable = exec_tools_info.exec_interpreter[DefaultInfo].files_to_run
        program_info = precompiler[PyInterpreterProgramInfo]
        env.update(program_info.env)
        precompiler_startup_args.add_all(program_info.interpreter_args)
        default_info = precompiler[DefaultInfo]
        precompiler_startup_args.add(default_info.files_to_run.executable)
        tools.append(default_info.files_to_run)
    elif precompiler[DefaultInfo].files_to_run:
        precompiler_executable = precompiler[DefaultInfo].files_to_run
    else:
        fail(("Unrecognized precompiler: target '{}' does not provide " +
              "PyInterpreterProgramInfo nor appears to be executable").format(
            precompiler,
        ))

    stem = src.basename[:-(len(src.extension) + 1)]
    if use_pycache:
        if not hasattr(target_toolchain, "pyc_tag") or not target_toolchain.pyc_tag:
            # This is likely one of two situations:
            # 1. The pyc_tag attribute is missing because it's the Bazel-builtin
            #    PyRuntimeInfo object.
            # 2. It's a "runtime toolchain", i.e. the autodetecting toolchain,
            #    or some equivalent toolchain that can't assume to know the
            #    runtime Python version at build time.
            # Instead of failing, just don't generate any pyc.
            return None
        pyc_path = "__pycache__/{stem}.{tag}.pyc".format(
            stem = stem,
            tag = target_toolchain.pyc_tag,
        )
    else:
        pyc_path = "{}.pyc".format(stem)

    pyc = ctx.actions.declare_file(pyc_path, sibling = src)

    invalidation_mode = ctx.attr.precompile_invalidation_mode
    if invalidation_mode == PrecompileInvalidationModeAttr.AUTO:
        if ctx.var["COMPILATION_MODE"] == "opt":
            invalidation_mode = PrecompileInvalidationModeAttr.UNCHECKED_HASH
        else:
            invalidation_mode = PrecompileInvalidationModeAttr.CHECKED_HASH

    # Though --modify_execution_info exists, it can only set keys with
    # empty values, which doesn't work for persistent worker settings.
    execution_requirements = {}
    if testing.ExecutionInfo in precompiler:
        execution_requirements.update(precompiler[testing.ExecutionInfo].requirements)

    # These args are passed for every precompilation request, e.g. as part of
    # a request to a worker process.
    precompile_request_args = ctx.actions.args()

    # Always use param files so that it can be run as a persistent worker
    precompile_request_args.use_param_file("@%s", use_always = True)
    precompile_request_args.set_param_file_format("multiline")

    precompile_request_args.add("--invalidation_mode", invalidation_mode)
    precompile_request_args.add("--src", src)

    # NOTE: src.short_path is used because src.path contains the platform and
    # build-specific hash portions of the path, which we don't want in the
    # pyc data. Note, however, for remote-remote files, short_path will
    # have the repo name, which is likely to contain extraneous info.
    precompile_request_args.add("--src_name", src.short_path)
    precompile_request_args.add("--pyc", pyc)
    precompile_request_args.add("--optimize", ctx.attr.precompile_optimize_level)

    version_info = target_toolchain.interpreter_version_info
    python_version = "{}.{}".format(version_info.major, version_info.minor)
    precompile_request_args.add("--python_version", python_version)

    ctx.actions.run(
        executable = precompiler_executable,
        arguments = [precompiler_startup_args, precompile_request_args],
        inputs = [src],
        outputs = [pyc],
        mnemonic = "PyCompile",
        progress_message = "Python precompiling %{input} into %{output}",
        tools = tools,
        env = env | {
            "PYTHONHASHSEED": "0",  # Helps avoid non-deterministic behavior
            "PYTHONNOUSERSITE": "1",  # Helps avoid non-deterministic behavior
            "PYTHONSAFEPATH": "1",  # Helps avoid incorrect import issues
        },
        execution_requirements = execution_requirements,
        toolchain = EXEC_TOOLS_TOOLCHAIN_TYPE,
    )
    return pyc
