# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""Common code for sh_binary and sh_test rules."""

visibility(["//shell"])

_SH_TOOLCHAIN_TYPE = Label("//shell:toolchain_type")

def _to_rlocation_path(ctx, file):
    if file.short_path.startswith("../"):
        return file.short_path[3:]
    else:
        return ctx.workspace_name + "/" + file.short_path

def _sh_executable_impl(ctx):
    if len(ctx.files.srcs) != 1:
        fail("you must specify exactly one file in 'srcs'", attr = "srcs")
    src = ctx.files.srcs[0]

    direct_files = [src]
    transitive_files = []
    runfiles = ctx.runfiles(collect_default = True)

    entrypoint = ctx.actions.declare_file(ctx.label.name)
    if ctx.attr.use_bash_launcher:
        ctx.actions.write(
            entrypoint,
            content = """#!{shell}

# --- begin runfiles.bash initialization v3 ---
set -uo pipefail; set +e; f=bazel_tools/tools/bash/runfiles/runfiles.bash
# shellcheck disable=SC1090
source "${{RUNFILES_DIR:-/dev/null}}/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "${{RUNFILES_MANIFEST_FILE:-/dev/null}}" | cut -f2- -d' ')" 2>/dev/null || \
  source "$0.runfiles/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.exe.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  {{ echo>&2 "ERROR: cannot find $f"; exit 1; }}; f=; set -e
# --- end runfiles.bash initialization v3 ---

runfiles_export_envvars

exec "$(rlocation "{src}")" "$@"
""".format(
                shell = ctx.toolchains[_SH_TOOLCHAIN_TYPE].path,
                src = _to_rlocation_path(ctx, src),
            ),
            is_executable = True,
        )
        runfiles = runfiles.merge(ctx.attr._runfiles_dep[DefaultInfo].default_runfiles)
    else:
        ctx.actions.symlink(
            output = entrypoint,
            target_file = src,
            is_executable = True,
        )

    direct_files.append(entrypoint)

    # TODO: Consider extracting this logic into a function provided by
    # sh_toolchain to allow users to inject launcher creation logic for
    # non-Windows platforms.
    if ctx.target_platform_has_constraint(ctx.attr._windows_constraint[platform_common.ConstraintValueInfo]):
        main_executable = _launcher_for_windows(ctx, entrypoint, src)
        direct_files.append(main_executable)
    else:
        main_executable = entrypoint

    files = depset(direct = direct_files, transitive = transitive_files)
    runfiles = runfiles.merge(ctx.runfiles(transitive_files = files))
    default_info = DefaultInfo(
        executable = main_executable,
        files = files,
        runfiles = runfiles,
    )

    instrumented_files_info = coverage_common.instrumented_files_info(
        ctx,
        source_attributes = ["srcs"],
        dependency_attributes = ["deps", "_runfiles_dep", "data"],
    )

    run_environment_info = RunEnvironmentInfo(
        environment = {
            key: ctx.expand_make_variables(
                "env",
                ctx.expand_location(value, ctx.attr.data, short_paths = True),
                {},
            )
            for key, value in ctx.attr.env.items()
        },
        inherited_environment = ctx.attr.env_inherit,
    )

    return [
        default_info,
        instrumented_files_info,
        run_environment_info,
    ]

_WINDOWS_EXECUTABLE_EXTENSIONS = [
    "exe",
    "cmd",
    "bat",
]

def _is_windows_executable(file):
    return file.extension in _WINDOWS_EXECUTABLE_EXTENSIONS

def _create_windows_exe_launcher(ctx, sh_toolchain, primary_output):
    if not sh_toolchain.launcher or not sh_toolchain.launcher_maker:
        fail("Windows sh_toolchain requires both 'launcher' and 'launcher_maker' to be set")

    bash_launcher = ctx.actions.declare_file(ctx.label.name + ".exe")

    launch_info = ctx.actions.args().use_param_file("%s", use_always = True).set_param_file_format("multiline")
    launch_info.add("binary_type=Bash")
    launch_info.add(ctx.workspace_name, format = "workspace_name=%s")
    launch_info.add("1" if ctx.configuration.runfiles_enabled() else "0", format = "symlink_runfiles_enabled=%s")
    launch_info.add(sh_toolchain.path, format = "bash_bin_path=%s")
    bash_file_short_path = primary_output.short_path
    if bash_file_short_path.startswith("../"):
        bash_file_rlocationpath = bash_file_short_path[3:]
    else:
        bash_file_rlocationpath = ctx.workspace_name + "/" + bash_file_short_path
    launch_info.add(bash_file_rlocationpath, format = "bash_file_rlocationpath=%s")

    launcher_artifact = sh_toolchain.launcher
    ctx.actions.run(
        executable = sh_toolchain.launcher_maker,
        inputs = [launcher_artifact],
        outputs = [bash_launcher],
        arguments = [launcher_artifact.path, launch_info, bash_launcher.path],
        use_default_shell_env = True,
        toolchain = _SH_TOOLCHAIN_TYPE,
    )
    return bash_launcher

def _launcher_for_windows(ctx, primary_output, main_file):
    if _is_windows_executable(main_file):
        if main_file.extension == primary_output.extension:
            return primary_output
        else:
            fail("Source file is a Windows executable file, target name extension should match source file extension")

    # bazel_tools should always registers a toolchain for Windows, but it may have an empty path.
    sh_toolchain = ctx.toolchains[_SH_TOOLCHAIN_TYPE]
    if not sh_toolchain or not sh_toolchain.path:
        # Let fail print the toolchain type with an apparent repo name.
        fail(
            """No suitable shell toolchain found:
* if you are running Bazel on Windows, set the BAZEL_SH environment variable to the path of bash.exe
* if you are running Bazel on a non-Windows platform but are targeting Windows, register an sh_toolchain for the""",
            _SH_TOOLCHAIN_TYPE,
            "toolchain type",
        )

    return _create_windows_exe_launcher(ctx, sh_toolchain, primary_output)

def make_sh_executable_rule(doc, extra_attrs = {}, **kwargs):
    return rule(
        _sh_executable_impl,
        doc = doc,
        attrs = {
            "srcs": attr.label_list(
                allow_files = True,
                doc = """
The file containing the shell script.
<p>
  This attribute must be a singleton list, whose element is the shell script.
  This script must be executable, and may be a source file or a generated file.
  All other files required at runtime (whether scripts or data) belong in the
  <code>data</code> attribute.
</p>
""",
            ),
            "data": attr.label_list(
                allow_files = True,
                flags = ["SKIP_CONSTRAINTS_OVERRIDE"],
            ),
            "deps": attr.label_list(
                allow_rules = ["sh_library"],
                doc = """
The list of "library" targets to be aggregated into this target.
See general comments about <code>deps</code>
at <a href="${link common-definitions#typical.deps}">Typical attributes defined by
most build rules</a>.
<p>
  This attribute should be used to list other <code>sh_library</code> rules that provide
  interpreted program source code depended on by the code in <code>srcs</code>. The files
  provided by these rules will be present among the <code>runfiles</code> of this target.
</p>
""",
            ),
            "_runfiles_dep": attr.label(
                default = Label("//shell/runfiles"),
            ),
            "env": attr.string_dict(),
            "env_inherit": attr.string_list(),
            "use_bash_launcher": attr.bool(
                doc = "Use a bash launcher initializing the runfiles library",
            ),
            "_windows_constraint": attr.label(
                default = "@platforms//os:windows",
            ),
        } | extra_attrs,
        toolchains = [
            config_common.toolchain_type(_SH_TOOLCHAIN_TYPE, mandatory = False),
        ],
        **kwargs
    )
