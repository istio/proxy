# Copyright 2023 Jeremy Volkman. All rights reserved.
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

"""Implementation of the py_wheel_library rule."""

load("@bazel_skylib//lib:paths.bzl", "paths")
load("//python:py_info.bzl", "PyInfo")
load(":providers.bzl", "PyWheelInfo")

def _py_wheel_library_impl(ctx):
    out = ctx.actions.declare_directory(ctx.attr.name)

    wheel_target = ctx.attr.wheel
    if PyWheelInfo in wheel_target:
        wheel_file = wheel_target[PyWheelInfo].wheel_file
        name_file = wheel_target[PyWheelInfo].name_file
    else:
        wheel_file = ctx.file.wheel
        name_file = None

    args = ctx.actions.args().use_param_file("--flagfile=%s")
    args.add("--wheel", wheel_file)
    args.add("--directory", out.path)
    args.add_all(ctx.files.patches, format_each = "--patch=%s")
    args.add_all(ctx.attr.patch_args, format_each = "--patch-arg=%s")
    args.add("--patch-tool", ctx.attr.patch_tool)

    tools = []
    inputs = [wheel_file] + ctx.files.patches
    if name_file:
        inputs.append(name_file)
        args.add("--wheel-name-file", name_file)

    if ctx.attr.patch_tool_target:
        args.add("--patch-tool-target", ctx.attr.patch_tool_target.files_to_run.executable)
        tools.append(ctx.executable.patch_tool_target)

    if ctx.attr.enable_implicit_namespace_pkgs:
        args.add("--enable-implicit-namespace-pkgs")

    # We apply patches in the same action as the extraction to minimize the
    # number of times we cache the wheel contents. If we were to split this
    # into 2 actions, then the wheel contents would be cached twice.
    ctx.actions.run(
        inputs = inputs,
        outputs = [out],
        executable = ctx.executable._tool,
        tools = tools,
        arguments = [args],
        # Set environment variables to make generated .pyc files reproducible.
        env = {
            "PYTHONHASHSEED": "0",
            "SOURCE_DATE_EPOCH": "315532800",
        },
        mnemonic = "WheelInstall",
        progress_message = "Installing %s" % ctx.file.wheel.basename,
    )

    has_py2_only_sources = ctx.attr.python_version == "PY2"
    has_py3_only_sources = ctx.attr.python_version == "PY3"
    if not has_py2_only_sources:
        for d in ctx.attr.deps:
            if d[PyInfo].has_py2_only_sources:
                has_py2_only_sources = True
                break
    if not has_py3_only_sources:
        for d in ctx.attr.deps:
            if d[PyInfo].has_py3_only_sources:
                has_py3_only_sources = True
                break

    # TODO: Is there a more correct way to get this runfiles-relative import path?
    imp = paths.join(
        ctx.label.workspace_name or ctx.workspace_name,  # Default to the local workspace.
        ctx.label.package,
        ctx.label.name,
        "site-packages",  # we put lib files in this subdirectory.
    )

    imports = depset(
        direct = [imp],
        transitive = [d[PyInfo].imports for d in ctx.attr.deps],
    )
    transitive_sources = depset(
        direct = [out],
        transitive = [dep[PyInfo].transitive_sources for dep in ctx.attr.deps if PyInfo in dep],
    )
    runfiles = ctx.runfiles(files = [out])
    for d in ctx.attr.deps:
        runfiles = runfiles.merge(d[DefaultInfo].default_runfiles)

    return [
        DefaultInfo(
            files = depset(direct = [out]),
            runfiles = runfiles,
        ),
        PyInfo(
            has_py2_only_sources = has_py2_only_sources,
            has_py3_only_sources = has_py3_only_sources,
            imports = imports,
            transitive_sources = transitive_sources,
            uses_shared_libraries = True,  # Docs say this is unused
        ),
    ]

py_wheel_library = rule(
    implementation = _py_wheel_library_impl,
    attrs = {
        "deps": attr.label_list(
            doc = "A list of this wheel's Python library dependencies.",
            providers = [DefaultInfo, PyInfo],
        ),
        "enable_implicit_namespace_pkgs": attr.bool(
            default = True,
            doc = """
If true, disables conversion of native namespace packages into pkg-util style namespace packages. When set all py_binary
and py_test targets must specify either `legacy_create_init=False` or the global Bazel option
`--incompatible_default_to_explicit_init_py` to prevent `__init__.py` being automatically generated in every directory.
This option is required to support some packages which cannot handle the conversion to pkg-util style.
            """,
        ),
        "patch_args": attr.string_list(
            default = ["-p0"],
            doc =
                "The arguments given to the patch tool. Defaults to -p0, " +
                "however -p1 will usually be needed for patches generated by " +
                "git. If multiple -p arguments are specified, the last one will take effect.",
        ),
        "patch_tool": attr.string(
            doc = "The patch(1) utility from the host to use. " +
                  "If set, overrides `patch_tool_target`. Please note that setting " +
                  "this means that builds are not completely hermetic.",
        ),
        "patch_tool_target": attr.label(
            executable = True,
            cfg = "exec",
            doc = "The label of the patch(1) utility to use. " +
                  "Only used if `patch_tool` is not set.",
        ),
        "patches": attr.label_list(
            allow_files = True,
            default = [],
            doc =
                "A list of files that are to be applied as patches after " +
                "extracting the archive. This will use the patch command line tool.",
        ),
        "python_version": attr.string(
            doc = "The python version required for this wheel ('PY2' or 'PY3')",
            values = ["PY2", "PY3", ""],
        ),
        "wheel": attr.label(
            doc = "The wheel file.",
            allow_single_file = [".whl"],
            mandatory = True,
        ),
        "_tool": attr.label(
            default = Label("//third_party/rules_pycross/pycross/private/tools:wheel_installer"),
            cfg = "exec",
            executable = True,
        ),
    },
)
