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
"""Common functionality between test/binary executables."""

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_skylib//lib:structs.bzl", "structs")
load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_python_internal//:rules_python_config.bzl", rp_config = "config")
load(":attr_builders.bzl", "attrb")
load(
    ":attributes.bzl",
    "AGNOSTIC_EXECUTABLE_ATTRS",
    "COMMON_ATTRS",
    "COVERAGE_ATTRS",
    "IMPORTS_ATTRS",
    "PY_SRCS_ATTRS",
    "PrecompileAttr",
    "PycCollectionAttr",
    "REQUIRED_EXEC_GROUP_BUILDERS",
    "apply_config_settings_attr",
)
load(":builders.bzl", "builders")
load(":cc_helper.bzl", "cc_helper")
load(
    ":common.bzl",
    "collect_cc_info",
    "collect_deps",
    "collect_imports",
    "collect_runfiles",
    "create_binary_semantics_struct",
    "create_cc_details_struct",
    "create_instrumented_files_info",
    "create_output_group_info",
    "create_py_info",
    "create_windows_exe_launcher",
    "csv",
    "filter_to_py_srcs",
    "is_bool",
    "maybe_create_repo_mapping",
    "relative_path",
    "runfiles_root_path",
    "target_platform_has_any_constraint",
)
load(":common_labels.bzl", "labels")
load(":flags.bzl", "BootstrapImplFlag", "VenvsUseDeclareSymlinkFlag", "read_possibly_native_flag")
load(":precompile.bzl", "maybe_precompile")
load(":py_cc_link_params_info.bzl", "PyCcLinkParamsInfo")
load(":py_executable_info.bzl", "PyExecutableInfo")
load(":py_info.bzl", "PyInfo", "VenvSymlinkKind")
load(":py_internal.bzl", "py_internal")
load(":py_runtime_info.bzl", "DEFAULT_STUB_SHEBANG")
load(":reexports.bzl", "BuiltinPyInfo", "BuiltinPyRuntimeInfo")
load(":rule_builders.bzl", "ruleb")
load(":toolchain_types.bzl", "EXEC_TOOLS_TOOLCHAIN_TYPE", "LAUNCHER_MAKER_TOOLCHAIN_TYPE", TOOLCHAIN_TYPE = "TARGET_TOOLCHAIN_TYPE")
load(":transition_labels.bzl", "TRANSITION_LABELS")
load(":venv_runfiles.bzl", "create_venv_app_files")

_py_builtins = py_internal
_EXTERNAL_PATH_PREFIX = "external"
_ZIP_RUNFILES_DIRECTORY_NAME = "runfiles"
_INIT_PY = "__init__.py"

# Non-Google-specific attributes for executables
# These attributes are for rules that accept Python sources.
EXECUTABLE_ATTRS = dicts.add(
    COMMON_ATTRS,
    AGNOSTIC_EXECUTABLE_ATTRS,
    PY_SRCS_ATTRS,
    IMPORTS_ATTRS,
    # starlark flags attributes
    {
        "_build_python_zip_flag": attr.label(default = "//python/config_settings:build_python_zip"),
        "_default_to_explicit_init_py_flag": attr.label(default = "//python/config_settings:incompatible_default_to_explicit_init_py"),
        "_python_import_all_repositories_flag": attr.label(default = "//python/config_settings:experimental_python_import_all_repositories"),
        "_python_path_flag": attr.label(default = "//python/config_settings:python_path"),
    },
    {
        "interpreter_args": lambda: attrb.StringList(
            doc = """
Arguments that are only applicable to the interpreter.

The args an interpreter supports are specific to the interpreter. For
CPython, see https://docs.python.org/3/using/cmdline.html.

:::{note}
Only supported for {obj}`--bootstrap_impl=script`. Ignored otherwise.
:::

:::{seealso}
The {any}`RULES_PYTHON_ADDITIONAL_INTERPRETER_ARGS` environment variable
:::

:::{versionadded} 1.3.0
:::
""",
        ),
        "legacy_create_init": lambda: attrb.Int(
            default = -1,
            values = [-1, 0, 1],
            doc = """\
Whether to implicitly create empty `__init__.py` files in the runfiles tree.
These are created in every directory containing Python source code or shared
libraries, and every parent directory of those directories, excluding the repo
root directory. The default, `-1` (auto), means true unless
`--incompatible_default_to_explicit_init_py` is used. If false, the user is
responsible for creating (possibly empty) `__init__.py` files and adding them to
the `srcs` of Python targets as required.
                                       """,
        ),
        # TODO(b/203567235): In the Java impl, any file is allowed. While marked
        # label, it is more treated as a string, and doesn't have to refer to
        # anything that exists because it gets treated as suffix-search string
        # over `srcs`.
        "main": lambda: attrb.Label(
            allow_single_file = True,
            doc = """\
Optional; the name of the source file that is the main entry point of the
application. This file must also be listed in `srcs`. If left unspecified,
`name`, with `.py` appended, is used instead. If `name` does not match any
filename in `srcs`, `main` must be specified.

This is mutually exclusive with {obj}`main_module`.
""",
        ),
        "main_module": lambda: attrb.String(
            doc = """
Module name to execute as the main program.

When set, `srcs` is not required, and it is assumed the module is
provided by a dependency.

See https://docs.python.org/3/using/cmdline.html#cmdoption-m for more
information about running modules as the main program.

This is mutually exclusive with {obj}`main`.

:::{versionadded} 1.3.0
:::
:::{versionchanged} 1.7.0
Support added for {obj}`--bootstrap_impl=system_python`.
:::
""",
        ),
        "pyc_collection": lambda: attrb.String(
            default = PycCollectionAttr.INHERIT,
            values = sorted(PycCollectionAttr.__members__.values()),
            doc = """
Determines whether pyc files from dependencies should be manually included.

Valid values are:
* `inherit`: Inherit the value from {flag}`--precompile`.
* `include_pyc`: Add implicitly generated pyc files from dependencies. i.e.
  pyc files for targets that specify {attr}`precompile="inherit"`.
* `disabled`: Don't add implicitly generated pyc files. Note that
  pyc files may still come from dependencies that enable precompiling at the
  target level.
""",
        ),
        "python_version": lambda: attrb.String(
            # TODO(b/203567235): In the Java impl, the default comes from
            # --python_version. Not clear what the Starlark equivalent is.
            doc = """
The Python version this target should use.

The value should be in `X.Y` or `X.Y.Z` (or compatible) format. If empty or
unspecified, the incoming configuration's {obj}`--python_version` flag is
inherited. For backwards compatibility, the values `PY2` and `PY3` are
accepted, but treated as an empty/unspecified value.

:::{note}
In order for the requested version to be used, there must be a
toolchain configured to match the Python version. If there isn't, then it
may be silently ignored, or an error may occur, depending on the toolchain
configuration.
:::

:::{versionchanged} 1.1.0

This attribute was changed from only accepting `PY2` and `PY3` values to
accepting arbitrary Python versions.
:::
""",
        ),
        # Required to opt-in to the transition feature.
        "_allowlist_function_transition": lambda: attrb.Label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
        "_bootstrap_impl_flag": lambda: attrb.Label(
            default = labels.BOOTSTRAP_IMPL,
            providers = [BuildSettingInfo],
        ),
        "_bootstrap_template": lambda: attrb.Label(
            allow_single_file = True,
            default = "@bazel_tools//tools/python:python_bootstrap_template.txt",
        ),
        "_build_data_writer": lambda: attrb.Label(
            default = "//python/private:build_data_writer",
            allow_files = True,
            cfg = "exec",
        ),
        "_debugger_flag": lambda: attrb.Label(
            default = "//python/private:debugger_if_target_config",
            providers = [PyInfo],
        ),
        "_launcher": lambda: attrb.Label(
            cfg = "target",
            # NOTE: This is an executable, but is only used for Windows. It
            # can't have executable=True because the backing target is an
            # empty target for other platforms.
            default = "//tools/launcher:launcher",
        ),
        "_python_version_flag": lambda: attrb.Label(
            default = labels.PYTHON_VERSION,
        ),
        "_uncachable_version_file": lambda: attrb.Label(
            default = "//python/private:uncachable_version_file",
            allow_files = True,
        ),
        "_venvs_use_declare_symlink_flag": lambda: attrb.Label(
            default = labels.VENVS_USE_DECLARE_SYMLINK,
            providers = [BuildSettingInfo],
        ),
        "_windows_constraints": lambda: attrb.LabelList(
            default = [
                "@platforms//os:windows",
            ],
        ),
        "_zipper": lambda: attrb.Label(
            cfg = "exec",
            executable = True,
            default = "@bazel_tools//tools/zip:zipper",
        ),
    },
    {
        "_windows_launcher_maker": lambda: attrb.Label(
            default = "@bazel_tools//tools/launcher:launcher_maker",
            cfg = "exec",
            executable = True,
        ),
    } if not rp_config.bazel_9_or_later else {},
)

def convert_legacy_create_init_to_int(kwargs):
    """Convert "legacy_create_init" key to int, in-place.

    Args:
        kwargs: The kwargs to modify. The key "legacy_create_init", if present
            and bool, will be converted to its integer value, in place.
    """
    if is_bool(kwargs.get("legacy_create_init")):
        kwargs["legacy_create_init"] = 1 if kwargs["legacy_create_init"] else 0

def py_executable_impl(ctx, *, is_test, inherited_environment):
    return py_executable_base_impl(
        ctx = ctx,
        semantics = create_binary_semantics(),
        is_test = is_test,
        inherited_environment = inherited_environment,
    )

def create_binary_semantics():
    return create_binary_semantics_struct(
        # keep-sorted start
        get_native_deps_dso_name = _get_native_deps_dso_name,
        should_build_native_deps_dso = lambda ctx: False,
        # keep-sorted end
    )

def _should_create_init_files(ctx):
    if ctx.attr.legacy_create_init == -1:
        return not read_possibly_native_flag(ctx, "default_to_explicit_init_py")
    else:
        return bool(ctx.attr.legacy_create_init)

def _create_executable(
        ctx,
        *,
        executable,
        main_py,
        imports,
        is_test,
        runtime_details,
        cc_details,
        native_deps_details,
        runfiles_details,
        extra_deps):
    _ = is_test, cc_details, native_deps_details  # @unused

    is_windows = target_platform_has_any_constraint(ctx, ctx.attr._windows_constraints)

    if is_windows:
        if not executable.extension == "exe":
            fail("Should not happen: somehow we are generating a non-.exe file on windows")
        base_executable_name = executable.basename[0:-4]
    else:
        base_executable_name = executable.basename

    venv = None

    # The check for stage2_bootstrap_template is to support legacy
    # BuiltinPyRuntimeInfo providers, which is likely to come from
    # @bazel_tools//tools/python:autodetecting_toolchain, the toolchain used
    # for workspace builds when no rules_python toolchain is configured.
    if (
        runtime_details.effective_runtime and
        hasattr(runtime_details.effective_runtime, "stage2_bootstrap_template")
    ):
        venv = _create_venv(
            ctx,
            output_prefix = base_executable_name,
            imports = imports,
            runtime_details = runtime_details,
            add_runfiles_root_to_sys_path = (
                "1" if BootstrapImplFlag.get_value(ctx) == BootstrapImplFlag.SYSTEM_PYTHON else "0"
            ),
            extra_deps = extra_deps,
        )

        stage2_bootstrap = _create_stage2_bootstrap(
            ctx,
            output_prefix = base_executable_name,
            output_sibling = executable,
            main_py = main_py,
            imports = imports,
            runtime_details = runtime_details,
            venv = venv,
            build_data_file = runfiles_details.build_data_file,
        )
        extra_runfiles = ctx.runfiles(
            [stage2_bootstrap] + (
                venv.files_without_interpreter if venv else []
            ),
        ).merge(venv.lib_runfiles)
        zip_main = _create_zip_main(
            ctx,
            stage2_bootstrap = stage2_bootstrap,
            runtime_details = runtime_details,
            venv = venv,
        )
    else:
        stage2_bootstrap = None
        extra_runfiles = ctx.runfiles()
        zip_main = ctx.actions.declare_file(base_executable_name + ".temp", sibling = executable)
        _create_stage1_bootstrap(
            ctx,
            output = zip_main,
            main_py = main_py,
            imports = imports,
            is_for_zip = True,
            runtime_details = runtime_details,
        )

    zip_file = ctx.actions.declare_file(base_executable_name + ".zip", sibling = executable)
    _create_zip_file(
        ctx,
        output = zip_file,
        zip_main = zip_main,
        runfiles = runfiles_details.runfiles_without_exe.merge(extra_runfiles),
    )

    extra_default_outputs = []

    # NOTE: --build_python_zip defaults to true on Windows
    build_zip_enabled = read_possibly_native_flag(ctx, "build_python_zip")

    # When --build_python_zip is enabled, then the zip file becomes
    # one of the default outputs.
    if build_zip_enabled:
        if not is_windows:
            # buildifier: disable=print
            print(
                """
======================================================================
WARNING: Target: {}
  The `--build_python_zip` flag and implicit zipapp output of `py_binary`
  and `py_test` is deprecated and will be removed in a future release.
  Switch to `py_zipapp_binary` or `py_zipapp_test`. For migration
  instructions and guide, see:

  https://github.com/bazel-contrib/rules_python/issues/3567
======================================================================
               """.rstrip().format(ctx.label),
            )

        extra_default_outputs.append(zip_file)

    # The logic here is a bit convoluted. Essentially, there are 3 types of
    # executables produced:
    # 1. (non-Windows) A bootstrap template based program.
    # 2. (non-Windows) A self-executable zip file of a bootstrap template based program.
    # 3. (Windows) A native Windows executable that finds and launches
    #    the actual underlying Bazel program (one of the above). Note that
    #    it implicitly assumes one of the above is located next to it, and
    #    that --build_python_zip defaults to true for Windows.

    should_create_executable_zip = False
    bootstrap_output = None
    if not is_windows:
        if build_zip_enabled:
            should_create_executable_zip = True
        else:
            bootstrap_output = executable
    else:
        create_windows_exe_launcher(
            ctx,
            output = executable,
            use_zip_file = build_zip_enabled,
            python_binary_path = runtime_details.executable_interpreter_path,
        )
        if not build_zip_enabled:
            # On Windows, the main executable has an "exe" extension, so
            # here we re-use the un-extensioned name for the bootstrap output.
            bootstrap_output = ctx.actions.declare_file(base_executable_name)

            # The launcher looks for the non-zip executable next to
            # itself, so add it to the default outputs.
            extra_default_outputs.append(bootstrap_output)

    if should_create_executable_zip:
        if bootstrap_output != None:
            fail("Should not occur: bootstrap_output should not be used " +
                 "when creating an executable zip")
        _create_executable_zip_file(
            ctx,
            output = executable,
            zip_file = zip_file,
            stage2_bootstrap = stage2_bootstrap,
            runtime_details = runtime_details,
            venv = venv,
        )
    elif bootstrap_output:
        _create_stage1_bootstrap(
            ctx,
            output = bootstrap_output,
            stage2_bootstrap = stage2_bootstrap,
            runtime_details = runtime_details,
            is_for_zip = False,
            imports = imports,
            main_py = main_py,
            venv = venv,
        )
    else:
        # Otherwise, this should be the Windows case of launcher + zip.
        # Double check this just to make sure.
        if not is_windows or not build_zip_enabled:
            fail(("Should not occur: The non-executable-zip and " +
                  "non-bootstrap-template case should have windows and zip " +
                  "both true, but got " +
                  "is_windows={is_windows} " +
                  "build_zip_enabled={build_zip_enabled}").format(
                is_windows = is_windows,
                build_zip_enabled = build_zip_enabled,
            ))

    app_runfiles = builders.RunfilesBuilder()
    app_runfiles.add(runfiles_details.app_runfiles)
    if venv:
        app_runfiles.add(venv.files_without_interpreter)
        app_runfiles.add(venv.lib_runfiles)

    # The interpreter is added this late in the process so that it isn't
    # added to the zipped files.
    if venv and venv.interpreter:
        extra_runfiles = extra_runfiles.merge(ctx.runfiles([venv.interpreter]))
    return struct(
        # depset[File] of additional files that should be included as default
        # outputs.
        extra_default_outputs = depset(extra_default_outputs),
        # dict[str, depset[File]]; additional output groups that should be
        # returned.
        output_groups = {"python_zip_file": depset([zip_file])},
        # runfiles; additional runfiles to include.
        extra_runfiles = extra_runfiles,
        # File|None; the stage2 bootstrap file, if any
        stage2_bootstrap = stage2_bootstrap,
        # runfiles; runfiles for the app itself (e.g its deps, but no Python
        # runtime files)
        app_runfiles = app_runfiles.build(ctx),
        # File|None; the venv `bin/python3` file, if any.
        venv_python_exe = venv.interpreter if venv else None,
    )

def _create_zip_main(ctx, *, stage2_bootstrap, runtime_details, venv):
    if venv.interpreter:
        python_binary = runfiles_root_path(ctx, venv.interpreter.short_path)
    else:
        python_binary = ""
    python_binary_actual = venv.interpreter_actual_path

    # The location of this file doesn't really matter. It's added to
    # the zip file as the top-level __main__.py file and not included
    # elsewhere.
    output = ctx.actions.declare_file(ctx.label.name + "_zip__main__.py")
    ctx.actions.expand_template(
        template = runtime_details.effective_runtime.zip_main_template,
        output = output,
        substitutions = {
            "%python_binary%": python_binary,
            "%python_binary_actual%": python_binary_actual,
            "%stage2_bootstrap%": "{}/{}".format(
                ctx.workspace_name,
                stage2_bootstrap.short_path,
            ),
            "%workspace_name%": ctx.workspace_name,
        },
    )
    return output

# Create a venv the executable can use.
# For venv details and the venv startup process, see:
# * https://docs.python.org/3/library/venv.html
# * https://snarky.ca/how-virtual-environments-work/
# * https://github.com/python/cpython/blob/main/Modules/getpath.py
# * https://github.com/python/cpython/blob/main/Lib/site.py
def _create_venv(ctx, output_prefix, imports, runtime_details, add_runfiles_root_to_sys_path, extra_deps):
    create_full_venv = BootstrapImplFlag.get_value(ctx) == BootstrapImplFlag.SCRIPT
    venv = "_{}.venv".format(output_prefix.lstrip("_"))

    if create_full_venv:
        # The pyvenv.cfg file must be present to trigger the venv site hooks.
        # Because it's paths are expected to be absolute paths, we can't reliably
        # put much in it. See https://github.com/python/cpython/issues/83650
        pyvenv_cfg = ctx.actions.declare_file("{}/pyvenv.cfg".format(venv))
        ctx.actions.write(pyvenv_cfg, "")
    else:
        pyvenv_cfg = None

    runtime = runtime_details.effective_runtime

    venvs_use_declare_symlink_enabled = (
        VenvsUseDeclareSymlinkFlag.get_value(ctx) == VenvsUseDeclareSymlinkFlag.YES
    )
    recreate_venv_at_runtime = False

    if runtime.interpreter:
        interpreter_actual_path = runfiles_root_path(ctx, runtime.interpreter.short_path)
    else:
        interpreter_actual_path = runtime.interpreter_path

    bin_dir = "{}/bin".format(venv)

    if create_full_venv:
        # Some wrappers around the interpreter (e.g. pyenv) use the program
        # name to decide what to do, so preserve the name.
        py_exe_basename = paths.basename(interpreter_actual_path)

        if not venvs_use_declare_symlink_enabled or not runtime.supports_build_time_venv:
            recreate_venv_at_runtime = True

            # When the venv symlinks are disabled, the $venv/bin/python3 file isn't
            # needed or used at runtime. However, the zip code uses the interpreter
            # File object to figure out some paths.
            interpreter = ctx.actions.declare_file("{}/{}".format(bin_dir, py_exe_basename))
            ctx.actions.write(interpreter, "actual:{}".format(interpreter_actual_path))

        elif runtime.interpreter:
            # Even though ctx.actions.symlink() is used, using
            # declare_symlink() is required to ensure that the resulting file
            # in runfiles is always a symlink. An RBE implementation, for example,
            # may choose to write what symlink() points to instead.
            interpreter = ctx.actions.declare_symlink("{}/{}".format(bin_dir, py_exe_basename))

            rel_path = relative_path(
                # dirname is necessary because a relative symlink is relative to
                # the directory the symlink resides within.
                from_ = paths.dirname(runfiles_root_path(ctx, interpreter.short_path)),
                to = interpreter_actual_path,
            )

            ctx.actions.symlink(output = interpreter, target_path = rel_path)
        else:
            interpreter = ctx.actions.declare_symlink("{}/{}".format(bin_dir, py_exe_basename))
            ctx.actions.symlink(output = interpreter, target_path = runtime.interpreter_path)
    else:
        interpreter = None

    if runtime.interpreter_version_info:
        version = "{}.{}".format(
            runtime.interpreter_version_info.major,
            runtime.interpreter_version_info.minor,
        )
    else:
        version_flag = ctx.attr._python_version_flag[config_common.FeatureFlagInfo].value
        version_flag_parts = version_flag.split(".")[0:2]
        version = "{}.{}".format(*version_flag_parts)

    # See site.py logic: free-threaded builds append "t" to the venv lib dir name
    if "t" in runtime.abi_flags:
        version += "t"

    venv_site_packages = "lib/python{}/site-packages".format(version)
    site_packages = "{}/{}".format(venv, venv_site_packages)
    pth = ctx.actions.declare_file("{}/bazel.pth".format(site_packages))
    ctx.actions.write(pth, "import _bazel_site_init\n")

    site_init = ctx.actions.declare_file("{}/_bazel_site_init.py".format(site_packages))
    computed_subs = ctx.actions.template_dict()
    computed_subs.add_joined("%imports%", imports, join_with = ":", map_each = _map_each_identity)
    ctx.actions.expand_template(
        template = runtime.site_init_template,
        output = site_init,
        substitutions = {
            "%add_runfiles_root_to_sys_path%": add_runfiles_root_to_sys_path,
            "%coverage_tool%": _get_coverage_tool_runfiles_path(ctx, runtime),
            "%import_all%": "True" if read_possibly_native_flag(ctx, "python_import_all_repositories") else "False",
            "%site_init_runfiles_path%": "{}/{}".format(ctx.workspace_name, site_init.short_path),
            "%workspace_name%": ctx.workspace_name,
        },
        computed_substitutions = computed_subs,
    )

    venv_dir_map = {
        VenvSymlinkKind.BIN: bin_dir,
        VenvSymlinkKind.LIB: site_packages,
    }
    venv_app_files = create_venv_app_files(
        ctx,
        deps = collect_deps(ctx, extra_deps),
        venv_dir_map = venv_dir_map,
    )

    files_without_interpreter = [pth, site_init] + venv_app_files.venv_files
    if pyvenv_cfg:
        files_without_interpreter.append(pyvenv_cfg)

    return struct(
        # File or None; the `bin/python3` executable in the venv.
        # None if a full venv isn't created.
        interpreter = interpreter,
        # bool; True if the venv should be recreated at runtime
        recreate_venv_at_runtime = recreate_venv_at_runtime,
        # Runfiles root relative path or absolute path
        interpreter_actual_path = interpreter_actual_path,
        files_without_interpreter = files_without_interpreter,
        # string; venv-relative path to the site-packages directory.
        venv_site_packages = venv_site_packages,
        # string; runfiles-root relative path to venv root.
        venv_root = runfiles_root_path(
            ctx,
            paths.join(
                py_internal.get_label_repo_runfiles_path(ctx.label),
                venv,
            ),
        ),
        # venv files for user library dependencies (files that are specific
        # to the executable bootstrap and python runtime aren't here).
        # `root_symlinks` should be used, otherwise, with symlinks files always go
        # to `_main` prefix, and binaries from non-root module become broken.
        lib_runfiles = ctx.runfiles(
            root_symlinks = venv_app_files.runfiles_symlinks,
        ),
    )

def _map_each_identity(v):
    return v

def _get_coverage_tool_runfiles_path(ctx, runtime):
    if (ctx.configuration.coverage_enabled and
        runtime and
        runtime.coverage_tool):
        return "{}/{}".format(
            ctx.workspace_name,
            runtime.coverage_tool.short_path,
        )
    else:
        return ""

def _create_stage2_bootstrap(
        ctx,
        *,
        output_prefix,
        output_sibling,
        main_py,
        imports,
        runtime_details,
        build_data_file,
        venv):
    output = ctx.actions.declare_file(
        # Prepend with underscore to prevent pytest from trying to
        # process the bootstrap for files starting with `test_`
        "_{}_stage2_bootstrap.py".format(output_prefix),
        sibling = output_sibling,
    )
    runtime = runtime_details.effective_runtime

    template = runtime.stage2_bootstrap_template

    if main_py:
        main_py_path = runfiles_root_path(ctx, main_py.short_path)
    else:
        main_py_path = ""

    ctx.actions.expand_template(
        template = template,
        output = output,
        substitutions = {
            "%build_data_file%": runfiles_root_path(ctx, build_data_file.short_path),
            "%coverage_instrumented%": str(int(ctx.configuration.coverage_enabled and ctx.coverage_instrumented())),
            "%coverage_tool%": _get_coverage_tool_runfiles_path(ctx, runtime),
            "%import_all%": "True" if read_possibly_native_flag(ctx, "python_import_all_repositories") else "False",
            "%imports%": ":".join(imports.to_list()),
            "%main%": main_py_path,
            "%main_module%": ctx.attr.main_module,
            "%target%": str(ctx.label),
            "%venv_rel_site_packages%": venv.venv_site_packages,
            "%venv_root%": venv.venv_root,
            "%workspace_name%": ctx.workspace_name,
        },
        is_executable = True,
    )
    return output

def _create_stage1_bootstrap(
        ctx,
        *,
        output,
        main_py = None,
        stage2_bootstrap = None,
        imports = None,
        is_for_zip,
        runtime_details,
        venv = None):
    """Create a legacy bootstrap script that is written in Python."""
    runtime = runtime_details.effective_runtime

    if venv:
        if venv.interpreter:
            python_binary_path = runfiles_root_path(ctx, venv.interpreter.short_path)
        else:
            python_binary_path = ""
    else:
        python_binary_path = runtime_details.executable_interpreter_path

    python_binary_actual = venv.interpreter_actual_path if venv else ""

    # Guard against the following:
    # * Runtime may be None on Windows due to the --python_path flag.
    # * Runtime may not have 'supports_build_time_venv' if a really old version is autoloaded
    #   on bazel 7.6.x.
    if runtime and getattr(runtime, "supports_build_time_venv", False):
        resolve_python_binary_at_runtime = "0"
    else:
        resolve_python_binary_at_runtime = "1"

    subs = {
        "%interpreter_args%": "\n".join(ctx.attr.interpreter_args),
        "%is_zipfile%": "1" if is_for_zip else "0",
        "%python_binary%": python_binary_path,
        "%python_binary_actual%": python_binary_actual,
        "%recreate_venv_at_runtime%": str(int(venv.recreate_venv_at_runtime)) if venv else "0",
        "%resolve_python_binary_at_runtime%": resolve_python_binary_at_runtime,
        "%target%": str(ctx.label),
        "%venv_rel_site_packages%": venv.venv_site_packages if venv else "",
        "%workspace_name%": ctx.workspace_name,
    }

    if stage2_bootstrap:
        subs["%stage2_bootstrap%"] = runfiles_root_path(ctx, stage2_bootstrap.short_path)
        template = runtime.bootstrap_template
        subs["%shebang%"] = runtime.stub_shebang
    elif not ctx.files.srcs:
        fail("mandatory 'srcs' files have not been provided")
    else:
        if (ctx.configuration.coverage_enabled and
            runtime and
            runtime.coverage_tool):
            coverage_tool_runfiles_path = "{}/{}".format(
                ctx.workspace_name,
                runtime.coverage_tool.short_path,
            )
        else:
            coverage_tool_runfiles_path = ""
        if runtime:
            subs["%shebang%"] = runtime.stub_shebang
            template = runtime.bootstrap_template
        else:
            subs["%shebang%"] = DEFAULT_STUB_SHEBANG
            template = ctx.file._bootstrap_template

        subs["%coverage_tool%"] = coverage_tool_runfiles_path
        subs["%import_all%"] = ("True" if read_possibly_native_flag(ctx, "python_import_all_repositories") else "False")
        subs["%imports%"] = ":".join(imports.to_list())
        subs["%main%"] = "{}/{}".format(ctx.workspace_name, main_py.short_path)

    ctx.actions.expand_template(
        template = template,
        output = output,
        substitutions = subs,
        is_executable = True,
    )

def _create_zip_file(ctx, *, output, zip_main, runfiles):
    """Create a Python zipapp (zip with __main__.py entry point)."""
    workspace_name = ctx.workspace_name
    legacy_external_runfiles = _py_builtins.get_legacy_external_runfiles(ctx)

    manifest = ctx.actions.args()
    manifest.use_param_file("@%s", use_always = True)
    manifest.set_param_file_format("multiline")

    manifest.add("__main__.py={}".format(zip_main.path))
    manifest.add("__init__.py=")
    manifest.add(
        "{}=".format(_get_zip_runfiles_path(_INIT_PY, workspace_name)),
    )

    def map_zip_empty_filenames(list_paths_cb):
        return [
            # FIXME @aignas 2025-12-06: what kind of paths do we expect here? Will they
            # ever start with `../` or `external`?
            _get_zip_runfiles_path_legacy(path, workspace_name, legacy_external_runfiles) + "="
            for path in list_paths_cb().to_list()
        ]

    manifest.add_all(
        # NOTE: Accessing runfiles.empty_filenames implicitly flattens the runfiles.
        # Smuggle a lambda in via a list to defer that flattening.
        [lambda: runfiles.empty_filenames],
        map_each = map_zip_empty_filenames,
        allow_closure = True,
    )

    def map_zip_runfiles(file):
        return (
            # NOTE: Use "+" for performance
            _get_zip_runfiles_path_legacy(file.short_path, workspace_name, legacy_external_runfiles) +
            "=" + file.path
        )

    manifest.add_all(runfiles.files, map_each = map_zip_runfiles, allow_closure = True)

    inputs = [zip_main]
    zip_repo_mapping_manifest = maybe_create_repo_mapping(
        ctx = ctx,
        runfiles = runfiles,
    )
    if zip_repo_mapping_manifest:
        manifest.add("{}/_repo_mapping={}".format(
            _ZIP_RUNFILES_DIRECTORY_NAME,
            zip_repo_mapping_manifest.path,
        ))
        inputs.append(zip_repo_mapping_manifest)

    zip_cli_args = ctx.actions.args()
    zip_cli_args.add("cC")
    zip_cli_args.add(output)

    ctx.actions.run(
        executable = ctx.executable._zipper,
        arguments = [zip_cli_args, manifest],
        inputs = depset(inputs, transitive = [runfiles.files]),
        outputs = [output],
        use_default_shell_env = True,
        mnemonic = "PythonZipper",
        progress_message = "Building Python zip: %{label}",
    )

def _get_zip_runfiles_path(path, workspace_name = ""):
    # NOTE @aignas 2025-12-06: This is to avoid the prefix checking in the very
    # trivial case that is always happening once per this function call

    # NOTE: Use "+" for performance
    if workspace_name:
        # NOTE: Use "+" for performance
        return _ZIP_RUNFILES_DIRECTORY_NAME + "/" + workspace_name + "/" + path
    else:
        return _ZIP_RUNFILES_DIRECTORY_NAME + "/" + path

def _get_zip_runfiles_path_legacy(path, workspace_name, legacy_external_runfiles):
    if legacy_external_runfiles and path.startswith(_EXTERNAL_PATH_PREFIX):
        return _get_zip_runfiles_path(path.removeprefix(_EXTERNAL_PATH_PREFIX))
    elif path.startswith("../"):
        # NOTE: External runfiles (artifacts in other repos) will have a leading
        # path component of "../" so that they refer outside the main workspace
        # directory and into the runfiles root. So we simplify it, e.g.
        # "workspace/../foo/bar" to simply "foo/bar".
        return _get_zip_runfiles_path(path[3:])
    else:
        return _get_zip_runfiles_path(path, workspace_name)

def _create_executable_zip_file(
        ctx,
        *,
        output,
        zip_file,
        stage2_bootstrap,
        runtime_details,
        venv):
    prelude = ctx.actions.declare_file(
        "{}_zip_prelude.sh".format(output.basename),
        sibling = output,
    )
    if stage2_bootstrap:
        _create_stage1_bootstrap(
            ctx,
            output = prelude,
            stage2_bootstrap = stage2_bootstrap,
            runtime_details = runtime_details,
            is_for_zip = True,
            venv = venv,
        )
    else:
        ctx.actions.write(prelude, "#!/usr/bin/env python3\n")

    ctx.actions.run_shell(
        command = "cat {prelude} {zip} > {output}".format(
            prelude = prelude.path,
            zip = zip_file.path,
            output = output.path,
        ),
        inputs = [prelude, zip_file],
        outputs = [output],
        use_default_shell_env = True,
        mnemonic = "PyBuildExecutableZip",
        progress_message = "Build Python zip executable: %{label}",
    )

def _get_cc_details_for_binary(ctx, extra_deps):
    cc_info = collect_cc_info(ctx, extra_deps = extra_deps)
    return create_cc_details_struct(
        cc_info_for_propagating = cc_info,
        cc_info_for_self_link = cc_info,
        cc_info_with_extra_link_time_libraries = None,
        extra_runfiles = ctx.runfiles(),
        # Though the rules require the CcToolchain, it isn't actually used.
        cc_toolchain = None,
        feature_config = None,
    )

def _get_interpreter_path(ctx, *, runtime, flag_interpreter_path):
    if runtime:
        if runtime.interpreter_path:
            interpreter_path = runtime.interpreter_path
        else:
            interpreter_path = "{}/{}".format(
                ctx.workspace_name,
                runtime.interpreter.short_path,
            )

            # NOTE: External runfiles (artifacts in other repos) will have a
            # leading path component of "../" so that they refer outside the
            # main workspace directory and into the runfiles root. By
            # normalizing, we simplify e.g. "workspace/../foo/bar" to simply
            # "foo/bar"
            interpreter_path = paths.normalize(interpreter_path)

    elif flag_interpreter_path:
        interpreter_path = flag_interpreter_path
    else:
        fail("Unable to determine interpreter path")

    return interpreter_path

def _get_native_deps_dso_name(ctx):
    _ = ctx  # @unused
    fail("Building native deps DSO not supported.")

def py_executable_base_impl(ctx, *, semantics, is_test, inherited_environment = []):
    """Base rule implementation for a Python executable.

    Google and Bazel call this common base and apply customizations using the
    semantics object.

    Args:
        ctx: The rule ctx
        semantics: BinarySemantics struct; see create_binary_semantics_struct()
        is_test: bool, True if the rule is a test rule (has `test=True`),
            False if not (has `executable=True`)
        inherited_environment: List of str; additional environment variable
            names that should be inherited from the runtime environment when the
            executable is run.
    Returns:
        DefaultInfo provider for the executable
    """
    _validate_executable(ctx)

    if not ctx.attr.main_module:
        main_py = determine_main(ctx)
    else:
        main_py = None
    direct_sources = filter_to_py_srcs(ctx.files.srcs)
    precompile_result = maybe_precompile(ctx, direct_sources)

    required_py_files = precompile_result.keep_srcs
    required_pyc_files = []
    implicit_pyc_files = []
    implicit_pyc_source_files = direct_sources

    if ctx.attr.precompile == PrecompileAttr.ENABLED:
        required_pyc_files.extend(precompile_result.pyc_files)
    else:
        implicit_pyc_files.extend(precompile_result.pyc_files)

    # Sourceless precompiled builds omit the main py file from outputs, so
    # main has to be pointed to the precompiled main instead.
    if (main_py not in precompile_result.keep_srcs and
        PycCollectionAttr.is_pyc_collection_enabled(ctx)):
        main_py = precompile_result.py_to_pyc_map[main_py]

    executable = _declare_executable_file(ctx)
    default_outputs = builders.DepsetBuilder()
    default_outputs.add(executable)
    default_outputs.add(precompile_result.keep_srcs)
    default_outputs.add(required_pyc_files)

    extra_deps = []

    # The debugger dependency should be prevented by select() config elsewhere,
    # but just to be safe, also guard against adding it to the output here.
    if not _is_tool_config(ctx):
        extra_deps.append(ctx.attr._debugger_flag)

    imports = collect_imports(ctx, extra_deps = extra_deps)
    runtime_details = _get_runtime_details(ctx)
    cc_details = _get_cc_details_for_binary(ctx, extra_deps = extra_deps)
    native_deps_details = _get_native_deps_details(
        ctx,
        semantics = semantics,
        cc_details = cc_details,
        is_test = is_test,
    )
    runfiles_details = _get_base_runfiles_for_binary(
        ctx,
        executable = executable,
        extra_deps = extra_deps,
        required_py_files = required_py_files,
        required_pyc_files = required_pyc_files,
        implicit_pyc_files = implicit_pyc_files,
        implicit_pyc_source_files = implicit_pyc_source_files,
        runtime_runfiles = runtime_details.runfiles,
        extra_common_runfiles = [
            cc_details.extra_runfiles,
            native_deps_details.runfiles,
        ],
    )
    exec_result = _create_executable(
        ctx,
        executable = executable,
        main_py = main_py,
        imports = imports,
        is_test = is_test,
        runtime_details = runtime_details,
        cc_details = cc_details,
        native_deps_details = native_deps_details,
        runfiles_details = runfiles_details,
        extra_deps = extra_deps,
    )
    default_outputs.add(exec_result.extra_default_outputs)

    extra_exec_runfiles = exec_result.extra_runfiles.merge(
        ctx.runfiles(transitive_files = exec_result.extra_default_outputs),
    )

    # Copy any existing fields in case of company patches.
    runfiles_details = struct(**(
        structs.to_dict(runfiles_details) | dict(
            default_runfiles = runfiles_details.default_runfiles.merge(extra_exec_runfiles),
            data_runfiles = runfiles_details.data_runfiles.merge(extra_exec_runfiles),
        )
    ))

    app_runfiles = exec_result.app_runfiles

    return _create_providers(
        ctx = ctx,
        executable = executable,
        runfiles_details = runfiles_details,
        main_py = main_py,
        imports = imports,
        original_sources = direct_sources,
        required_py_files = required_py_files,
        required_pyc_files = required_pyc_files,
        implicit_pyc_files = implicit_pyc_files,
        implicit_pyc_source_files = implicit_pyc_source_files,
        default_outputs = default_outputs.build(),
        runtime_details = runtime_details,
        cc_info = cc_details.cc_info_for_propagating,
        inherited_environment = inherited_environment,
        output_groups = exec_result.output_groups,
        stage2_bootstrap = exec_result.stage2_bootstrap,
        app_runfiles = app_runfiles,
        venv_python_exe = exec_result.venv_python_exe,
        interpreter_args = ctx.attr.interpreter_args,
    )

def _get_build_info(ctx, cc_toolchain):
    build_info_files = py_internal.cc_toolchain_build_info_files(cc_toolchain)
    if cc_helper.is_stamping_enabled(ctx):
        # Makes the target depend on BUILD_INFO_KEY, which helps to discover stamped targets
        # See b/326620485 for more details.
        ctx.version_file  # buildifier: disable=no-effect
        return build_info_files.non_redacted_build_info_files.to_list()
    else:
        return build_info_files.redacted_build_info_files.to_list()

def _validate_executable(ctx):
    if ctx.attr.python_version == "PY2":
        fail("It is not allowed to use Python 2")

    if ctx.attr.main and ctx.attr.main_module:
        fail((
            "Only one of main and main_module can be set, got: " +
            "main={}, main_module={}"
        ).format(ctx.attr.main, ctx.attr.main_module))

def _declare_executable_file(ctx):
    if target_platform_has_any_constraint(ctx, ctx.attr._windows_constraints):
        executable = ctx.actions.declare_file(ctx.label.name + ".exe")
    else:
        executable = ctx.actions.declare_file(ctx.label.name)

    return executable

def _get_runtime_details(ctx):
    """Gets various information about the Python runtime to use.

    While most information comes from the toolchain, various legacy and
    compatibility behaviors require computing some other information.

    Args:
        ctx: Rule ctx

    Returns:
        A struct; see inline-field comments of the return value for details.
    """

    # Bazel has --python_path. This flag has a computed default of "python" when
    # its actual default is null (see
    # TOOD(bazelbuild/bazel#7901): Remove this once --python_path flag is removed.

    flag_interpreter_path = read_possibly_native_flag(ctx, "python_path")
    if not flag_interpreter_path.startswith("python") and not paths.is_absolute(flag_interpreter_path):
        fail("'python_path' must be an absolute path or a name to be resolved from the system PATH (e.g., 'python', 'python3').")

    effective_runtime = _maybe_get_runtime_from_ctx(ctx)

    if effective_runtime:
        direct = []  # List of files
        transitive = []  # List of depsets
        if effective_runtime.interpreter:
            direct.append(effective_runtime.interpreter)
            transitive.append(effective_runtime.files)

        if ctx.configuration.coverage_enabled:
            if effective_runtime.coverage_tool:
                direct.append(effective_runtime.coverage_tool)
            if effective_runtime.coverage_files:
                transitive.append(effective_runtime.coverage_files)
        runtime_files = depset(direct = direct, transitive = transitive)
    else:
        runtime_files = depset()

    executable_interpreter_path = _get_interpreter_path(
        ctx,
        runtime = effective_runtime,
        flag_interpreter_path = flag_interpreter_path,
    )

    return struct(
        # Optional PyRuntimeInfo: The runtime that should be used.
        # If None, it's probably Windows using the legacy auto-detecting toolchain
        # that acts as if no toolchain was found.
        effective_runtime = effective_runtime,
        # str; Path to the Python interpreter to use for running the executable
        # itself (not the bootstrap script). Either an absolute path (which
        # means it is platform-specific), or a runfiles-relative path (which
        # means the interpreter should be within `runtime_files`)
        executable_interpreter_path = executable_interpreter_path,
        # runfiles: Additional runfiles specific to the runtime that should
        # be included. For in-build runtimes, this shold include the interpreter
        # and any supporting files.
        runfiles = ctx.runfiles(transitive_files = runtime_files),
    )

def _maybe_get_runtime_from_ctx(ctx):
    """Finds the PyRuntimeInfo from the toolchain or attribute, if available.

    Returns:
        A PyRuntimeInfo provider, or None.
    """
    toolchain = ctx.toolchains[TOOLCHAIN_TYPE]

    if not hasattr(toolchain, "py3_runtime"):
        fail("Python toolchain field 'py3_runtime' is missing")
    if not toolchain.py3_runtime:
        fail("Python toolchain missing py3_runtime")
    py3_runtime = toolchain.py3_runtime

    # Hack around the fact that the autodetecting Python toolchain, which is
    # automatically registered, does not yet support Windows. In this case,
    # we want to return null so that _get_interpreter_path falls back on
    # --python_path. See tools/python/toolchain.bzl.
    # TODO(#7844): Remove this hack when the autodetecting toolchain has a
    # Windows implementation.
    if py3_runtime.interpreter_path == "/_magic_pyruntime_sentinel_do_not_use":
        return None

    if py3_runtime.python_version != "PY3":
        fail("Python toolchain py3_runtime must be python_version=PY3, got {}".format(
            py3_runtime.python_version,
        ))
    return py3_runtime

def _get_base_runfiles_for_binary(
        ctx,
        *,
        executable,
        extra_deps,
        required_py_files,
        required_pyc_files,
        implicit_pyc_files,
        implicit_pyc_source_files,
        runtime_runfiles,
        extra_common_runfiles):
    """Returns the set of runfiles necessary prior to executable creation.

    NOTE: The term "common runfiles" refers to the runfiles that are common to
        runfiles_without_exe, default_runfiles, and data_runfiles.

    Args:
        ctx: The rule ctx.
        executable: The main executable output.
        extra_deps: List of Targets; additional targets whose runfiles
            will be added to the common runfiles.
        required_py_files: `depset[File]` the direct, `.py` sources for the
            target that **must** be included by downstream targets. This should
            only be Python source files. It should not include pyc files.
        required_pyc_files: `depset[File]` the direct `.pyc` files this target
            produces.
        implicit_pyc_files: `depset[File]` pyc files that are only used if pyc
            collection is enabled.
        implicit_pyc_source_files: `depset[File]` source files for implicit pyc
            files that are used when the implicit pyc files are not.
        runtime_runfiles: runfiles for the python runtime.
        extra_common_runfiles: List of runfiles; additional runfiles that
            will be added to the common runfiles.

    Returns:
        struct with attributes:
        * default_runfiles: The default runfiles
        * data_runfiles: The data runfiles
        * runfiles_without_exe: The default runfiles, but without the executable
          or files specific to the original program/executable.
        * build_data_file: A file with build stamp information if stamping is
          enabled, otherwise None.
        * app_runfiles: Runfiles for user-space dependencies (doesn't
          include the runtime or build data files)
    """
    app_runfiles = builders.RunfilesBuilder()
    app_runfiles.files.add(required_py_files)
    app_runfiles.files.add(required_pyc_files)
    pyc_collection_enabled = PycCollectionAttr.is_pyc_collection_enabled(ctx)
    if pyc_collection_enabled:
        app_runfiles.files.add(implicit_pyc_files)
    else:
        app_runfiles.files.add(implicit_pyc_source_files)

    for dep in (ctx.attr.deps + extra_deps):
        if not (PyInfo in dep or (BuiltinPyInfo != None and BuiltinPyInfo in dep)):
            continue
        info = dep[PyInfo] if PyInfo in dep else dep[BuiltinPyInfo]
        app_runfiles.files.add(info.transitive_sources)

        # Everything past this won't work with BuiltinPyInfo
        if not hasattr(info, "transitive_pyc_files"):
            continue

        app_runfiles.files.add(info.transitive_pyc_files)
        if pyc_collection_enabled:
            app_runfiles.files.add(info.transitive_implicit_pyc_files)
        else:
            app_runfiles.files.add(info.transitive_implicit_pyc_source_files)

    app_runfiles.runfiles.append(collect_runfiles(ctx))
    if extra_deps:
        app_runfiles.add_targets(extra_deps)
    app_runfiles.add(extra_common_runfiles)

    app_runfiles = app_runfiles.build(ctx)

    if _should_create_init_files(ctx):
        app_runfiles = _py_builtins.merge_runfiles_with_generated_inits_empty_files_supplier(
            ctx = ctx,
            runfiles = app_runfiles,
        )

    runfiles_without_exe = builders.RunfilesBuilder()
    runfiles_without_exe.add(app_runfiles)
    runfiles_without_exe.add(runtime_runfiles)
    build_data_file = _write_build_data(ctx)
    runfiles_without_exe.add(build_data_file)

    runfiles_without_exe = runfiles_without_exe.build(ctx)

    runfiles_with_exe = runfiles_without_exe.merge(ctx.runfiles([executable]))

    # There are three types of runfiles:
    # 1. app: Deps added by a user. This is akin to the typical files that would
    #    be in a traditional venv. No Python runtime files or build data files.
    # 2. without-exe: (1) + build data + python runtime
    # 3. binary (default/data runfiles): (2) + main executable
    return struct(
        app_runfiles = app_runfiles,
        build_data_file = build_data_file,
        data_runfiles = runfiles_with_exe,
        default_runfiles = runfiles_with_exe,
        runfiles_without_exe = runfiles_without_exe,
    )

def _write_build_data(ctx):
    inputs = builders.DepsetBuilder()
    if is_stamping_enabled(ctx):
        # NOTE: ctx.info_file is undocumented; see
        # https://github.com/bazelbuild/bazel/issues/9363
        info_file = ctx.info_file
        version_file = ctx.files._uncachable_version_file[0]
        inputs.add(info_file)
        inputs.add(version_file)
    else:
        info_file = None
        version_file = None

    # A "constant metadata" file is basically a special file that doesn't
    # support change detection logic and reports that it is unchanged. i.e., it
    # behaves like ctx.version_file and is ignored when computing "what inputs
    # changed" (see https://bazel.build/docs/user-manual#workspace-status).
    #
    # We do this so that consumers of the final build data file don't have
    # to transitively rebuild everything -- the `uncachable_version_file` file
    # isn't cachable, which causes the build data action to always re-run.
    #
    # While this technically means a binary could have stale build info,
    # it ends up not mattering in practice because the volatile information
    # doesn't meaningfully effect other outputs.
    #
    # This is also done for performance and Make It work reasons:
    #   * Passing the transitive dependencies into the action requires passing
    #     the runfiles, but actions don't directly accept runfiles. While
    #     flattening the depsets can be deferred, accessing the
    #     `runfiles.empty_filenames` attribute will will invoke the empty
    #     file supplier a second time, which is too much of a memory and CPU
    #     performance hit.
    #   * Some targets specify a directory in `data`, which is unsound, but
    #     mostly works. Google's RBE, unfortunately, rejects it.
    #   * A binary's transitive closure may be so large that it exceeds
    #     Google RBE limits for action inputs.
    build_data = _py_builtins.declare_constant_metadata_file(
        ctx = ctx,
        name = ctx.label.name + ".build_data.txt",
        root = ctx.bin_dir,
    )

    action_args = ctx.actions.args()
    writer_file = ctx.files._build_data_writer[0]
    if writer_file.path.endswith(".ps1"):
        # powershell.exe is used for broader compatibility
        # It is installed by default on most Windows versions
        action_exe = "powershell.exe"
        action_args.add_all([
            # Bypass execution policy is needed because,
            # by default, Windows blocks ps1 scripts.
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            writer_file,
        ])
        inputs.add(writer_file)
    else:
        action_exe = ctx.attr._build_data_writer[DefaultInfo].files_to_run

    ctx.actions.run(
        executable = action_exe,
        arguments = [action_args],
        env = {
            # Include config mode so that binaries can detect if they're
            # being used as a build tool or not, allowing for runtime optimizations.
            "CONFIG_MODE": "EXEC" if _is_tool_config(ctx) else "TARGET",
            "INFO_FILE": info_file.path if info_file else "",
            "OUTPUT": build_data.path,
            # Include this so it's explicit, otherwise, one has to detect
            # this by looking for the absense of info_file keys.
            "STAMPED": "TRUE" if is_stamping_enabled(ctx) else "FALSE",
            "TARGET": str(ctx.label),
            "VERSION_FILE": version_file.path if version_file else "",
        },
        inputs = inputs.build(),
        outputs = [build_data],
        mnemonic = "PyWriteBuildData",
        progress_message = "Reticulating %{label} build data",
        toolchain = None,
    )
    return build_data

def _get_native_deps_details(ctx, *, semantics, cc_details, is_test):
    if not semantics.should_build_native_deps_dso(ctx):
        return struct(dso = None, runfiles = ctx.runfiles())

    cc_info = cc_details.cc_info_for_self_link

    if not cc_info.linking_context.linker_inputs:
        return struct(dso = None, runfiles = ctx.runfiles())

    dso = ctx.actions.declare_file(semantics.get_native_deps_dso_name(ctx))
    share_native_deps = py_internal.share_native_deps(ctx)
    cc_feature_config = cc_details.feature_config
    if share_native_deps:
        linked_lib = _create_shared_native_deps_dso(
            ctx,
            cc_info = cc_info,
            is_test = is_test,
            requested_features = cc_feature_config.requested_features,
            feature_configuration = cc_feature_config.feature_configuration,
            cc_toolchain = cc_details.cc_toolchain,
        )
        ctx.actions.symlink(
            output = dso,
            target_file = linked_lib,
            progress_message = "Symlinking shared native deps for %{label}",
        )
    else:
        linked_lib = dso

    # The regular cc_common.link API can't be used because several
    # args are private-use only; see # private comments
    py_internal.link(
        name = ctx.label.name,
        actions = ctx.actions,
        linking_contexts = [cc_info.linking_context],
        output_type = "dynamic_library",
        never_link = True,  # private
        native_deps = True,  # private
        feature_configuration = cc_feature_config.feature_configuration,
        cc_toolchain = cc_details.cc_toolchain,
        test_only_target = is_test,  # private
        stamp = 1 if is_stamping_enabled(ctx) else 0,
        main_output = linked_lib,  # private
        use_shareable_artifact_factory = True,  # private
    )
    return struct(
        dso = dso,
        runfiles = ctx.runfiles(files = [dso]),
    )

def _create_shared_native_deps_dso(
        ctx,
        *,
        cc_info,
        is_test,
        feature_configuration,
        requested_features,
        cc_toolchain):
    linkstamps = [
        py_internal.linkstamp_file(linkstamp)
        for linker_input in cc_info.linking_context.linker_inputs.to_list()
        for linkstamp in linker_input.linkstamps
    ]

    partially_disabled_thin_lto = (
        cc_common.is_enabled(
            feature_name = "thin_lto_linkstatic_tests_use_shared_nonlto_backends",
            feature_configuration = feature_configuration,
        ) and not cc_common.is_enabled(
            feature_name = "thin_lto_all_linkstatic_use_shared_nonlto_backends",
            feature_configuration = feature_configuration,
        )
    )
    dso_hash = _get_shared_native_deps_hash(
        linker_inputs = cc_helper.get_static_mode_params_for_dynamic_library_libraries(
            depset([
                lib
                for linker_input in cc_info.linking_context.linker_inputs.to_list()
                for lib in linker_input.libraries
            ]),
        ),
        link_opts = [
            flag
            for input in cc_info.linking_context.linker_inputs.to_list()
            for flag in input.user_link_flags
        ],
        linkstamps = linkstamps,
        build_info_artifacts = _get_build_info(ctx, cc_toolchain) if linkstamps else [],
        features = requested_features,
        is_test_target_partially_disabled_thin_lto = is_test and partially_disabled_thin_lto,
    )
    return py_internal.declare_shareable_artifact(ctx, "_nativedeps/%x.so" % dso_hash)

# This is a minimal version of NativeDepsHelper.getSharedNativeDepsPath, see
# com.google.devtools.build.lib.rules.nativedeps.NativeDepsHelper#getSharedNativeDepsPath
# The basic idea is to take all the inputs that affect linking and encode (via
# hashing) them into the filename.
# TODO(b/234232820): The settings that affect linking must be kept in sync with the actual
# C++ link action. For more information, see the large descriptive comment on
# NativeDepsHelper#getSharedNativeDepsPath.
def _get_shared_native_deps_hash(
        *,
        linker_inputs,
        link_opts,
        linkstamps,
        build_info_artifacts,
        features,
        is_test_target_partially_disabled_thin_lto):
    # NOTE: We use short_path because the build configuration root in which
    # files are always created already captures the configuration-specific
    # parts, so no need to include them manually.
    parts = []
    for artifact in linker_inputs:
        parts.append(artifact.short_path)
    parts.append(str(len(link_opts)))
    parts.extend(link_opts)
    for artifact in linkstamps:
        parts.append(artifact.short_path)
    for artifact in build_info_artifacts:
        parts.append(artifact.short_path)
    parts.extend(sorted(features))

    # Sharing of native dependencies may cause an {@link
    # ActionConflictException} when ThinLTO is disabled for test and test-only
    # targets that are statically linked, but enabled for other statically
    # linked targets. This happens in case the artifacts for the shared native
    # dependency are output by {@link Action}s owned by the non-test and test
    # targets both. To fix this, we allow creation of multiple artifacts for the
    # shared native library - one shared among the test and test-only targets
    # where ThinLTO is disabled, and the other shared among other targets where
    # ThinLTO is enabled. See b/138118275
    parts.append("1" if is_test_target_partially_disabled_thin_lto else "0")

    return hash("".join(parts))

def determine_main(ctx):
    """Determine the main entry point .py source file.

    Args:
        ctx: The rule ctx.

    Returns:
        Artifact; the main file. If one can't be found, an error is raised.
    """
    if ctx.attr.main:
        proposed_main = ctx.attr.main.label.name
        if not proposed_main.endswith(".py"):
            fail("main must end in '.py'")
    else:
        if ctx.label.name.endswith(".py"):
            fail("name must not end in '.py'")
        proposed_main = ctx.label.name + ".py"

    main_files = [src for src in ctx.files.srcs if _path_endswith(src.short_path, proposed_main)]
    if not main_files:
        if ctx.attr.main:
            fail("could not find '{}' as specified by 'main' attribute".format(proposed_main))
        else:
            fail(("corresponding default '{}' does not appear in srcs. Add " +
                  "it or override default file name with a 'main' attribute").format(
                proposed_main,
            ))

    elif len(main_files) > 1:
        if ctx.attr.main:
            fail(("file name '{}' specified by 'main' attributes matches multiple files. " +
                  "Matches: {}").format(
                proposed_main,
                csv([f.short_path for f in main_files]),
            ))
        else:
            fail(("default main file '{}' matches multiple files in srcs. Perhaps specify " +
                  "an explicit file with 'main' attribute? Matches were: {}").format(
                proposed_main,
                csv([f.short_path for f in main_files]),
            ))
    return main_files[0]

def _path_endswith(path, endswith):
    # Use slash to anchor each path to prevent e.g.
    # "ab/c.py".endswith("b/c.py") from incorrectly matching.
    return ("/" + path).endswith("/" + endswith)

def is_stamping_enabled(ctx):
    """Tells if stamping is enabled or not.

    Args:
        ctx: The rule ctx
    Returns:
        bool; True if stamping is enabled, False if not.
    """

    # Always ignore stamping for exec config. This mitigates stamping
    # invalidating build action caching.
    if _is_tool_config(ctx):
        return False

    stamp = ctx.attr.stamp
    if stamp == 1:
        return True
    elif stamp == 0:
        return False
    elif stamp == -1:
        # NOTE: ctx.configuration.stamp_binaries() exposes this, but that's
        # a private API. To workaround, it'd been eposed via py_internal.
        return py_internal.stamp_binaries(ctx)
    else:
        fail("Unsupported `stamp` value: {}".format(stamp))

def _is_tool_config(ctx):
    # NOTE: The is_tool_configuration() function is only usable by builtins.
    # See https://github.com/bazelbuild/bazel/issues/14444 for the FR for
    # a more public API. Until that's available, py_internal to the rescue.
    return py_internal.is_tool_configuration(ctx)

def _create_providers(
        *,
        ctx,
        executable,
        main_py,
        original_sources,
        required_py_files,
        required_pyc_files,
        implicit_pyc_files,
        implicit_pyc_source_files,
        default_outputs,
        runfiles_details,
        imports,
        cc_info,
        inherited_environment,
        runtime_details,
        output_groups,
        stage2_bootstrap,
        app_runfiles,
        venv_python_exe,
        interpreter_args):
    """Creates the providers an executable should return.

    Args:
        ctx: The rule ctx.
        executable: File; the target's executable file.
        main_py: File; the main .py entry point.
        original_sources: `depset[File]` the direct `.py` sources for the
            target that were the original input sources.
        required_py_files: `depset[File]` the direct, `.py` sources for the
            target that **must** be included by downstream targets. This should
            only be Python source files. It should not include pyc files.
        required_pyc_files: `depset[File]` the direct `.pyc` files this target
            produces.
        implicit_pyc_files: `depset[File]` pyc files that are only used if pyc
            collection is enabled.
        implicit_pyc_source_files: `depset[File]` source files for implicit pyc
            files that are used when the implicit pyc files are not.
        default_outputs: depset of Files; the files for DefaultInfo.files
        runfiles_details: runfiles that will become the default  and data runfiles.
        imports: depset of strings; the import paths to propagate
        cc_info: optional CcInfo; Linking information to propagate as
            PyCcLinkParamsInfo. Note that only the linking information
            is propagated, not the whole CcInfo.
        inherited_environment: list of strings; Environment variable names
            that should be inherited from the environment the executuble
            is run within.
        runtime_details: struct of runtime information; see _get_runtime_details()
        output_groups: dict[str, depset[File]]; used to create OutputGroupInfo
        stage2_bootstrap: File; the stage 2 bootstrap script.
        app_runfiles: runfiles; the runfiles for the application (deps, etc).
        venv_python_exe: File; the python executable in the venv.
        interpreter_args: list of strings; arguments to pass to the interpreter.

    Returns:
        A list of modern providers.
    """
    providers = [
        DefaultInfo(
            executable = executable,
            files = default_outputs,
            default_runfiles = _py_builtins.make_runfiles_respect_legacy_external_runfiles(
                ctx,
                runfiles_details.default_runfiles,
            ),
            data_runfiles = _py_builtins.make_runfiles_respect_legacy_external_runfiles(
                ctx,
                runfiles_details.data_runfiles,
            ),
        ),
        create_instrumented_files_info(ctx),
        _create_run_environment_info(ctx, inherited_environment),
        PyExecutableInfo(
            main = main_py,
            runfiles_without_exe = runfiles_details.runfiles_without_exe,
            build_data_file = runfiles_details.build_data_file,
            interpreter_path = runtime_details.executable_interpreter_path,
            stage2_bootstrap = stage2_bootstrap,
            app_runfiles = app_runfiles,
            venv_python_exe = venv_python_exe,
            interpreter_args = interpreter_args,
        ),
    ]

    # TODO - The effective runtime can be None for Windows + auto detecting toolchain.
    # This can be removed once that's fixed; see maybe_get_runtime_from_ctx().
    if runtime_details.effective_runtime:
        py_runtime_info = runtime_details.effective_runtime
        providers.append(py_runtime_info)

        # Re-add the builtin PyRuntimeInfo for compatibility to make
        # transitioning easier, but only if it isn't already added because
        # returning the same provider type multiple times is an error.
        # NOTE: The PyRuntimeInfo from the toolchain could be a rules_python
        # PyRuntimeInfo or a builtin PyRuntimeInfo -- a user could have used the
        # builtin py_runtime rule or defined their own. We can't directly detect
        # the type of the provider object, but the rules_python PyRuntimeInfo
        # object has an extra attribute that the builtin one doesn't.
        if hasattr(py_runtime_info, "interpreter_version_info") and BuiltinPyRuntimeInfo != None:
            providers.append(BuiltinPyRuntimeInfo(
                interpreter_path = py_runtime_info.interpreter_path,
                interpreter = py_runtime_info.interpreter,
                files = py_runtime_info.files,
                coverage_tool = py_runtime_info.coverage_tool,
                coverage_files = py_runtime_info.coverage_files,
                python_version = py_runtime_info.python_version,
                stub_shebang = py_runtime_info.stub_shebang,
                bootstrap_template = py_runtime_info.bootstrap_template,
            ))

    # TODO(b/163083591): Remove the PyCcLinkParamsInfo once binaries-in-deps
    # are cleaned up.
    if cc_info:
        providers.append(
            PyCcLinkParamsInfo(cc_info = cc_info),
        )

    py_info, builtin_py_info = create_py_info(
        ctx,
        original_sources = original_sources,
        required_py_files = required_py_files,
        required_pyc_files = required_pyc_files,
        implicit_pyc_files = implicit_pyc_files,
        implicit_pyc_source_files = implicit_pyc_source_files,
        imports = imports,
    )

    providers.append(py_info)
    if builtin_py_info:
        providers.append(builtin_py_info)
    providers.append(create_output_group_info(py_info.transitive_sources, output_groups))
    return providers

def _create_run_environment_info(ctx, inherited_environment):
    expanded_env = {}
    for key, value in ctx.attr.env.items():
        expanded_env[key] = _py_builtins.expand_location_and_make_variables(
            ctx = ctx,
            attribute_name = "env[{}]".format(key),
            expression = value,
            targets = ctx.attr.data,
        )
    if "PYTHONBREAKPOINT" not in inherited_environment:
        inherited_environment = inherited_environment + ["PYTHONBREAKPOINT"]
    return RunEnvironmentInfo(
        environment = expanded_env,
        inherited_environment = inherited_environment,
    )

def _add_config_setting_defaults(kwargs):
    config_settings = kwargs.get("config_settings", None)
    if config_settings == None:
        config_settings = {}

    # NOTE: This code runs in loading phase within the context of the caller.
    # Label() must be used to resolve repo names within rules_python's
    # context to avoid unknown repo name errors.
    default = select({
        labels.PLATFORMS_OS_WINDOWS: {
            labels.ENABLE_RUNFILES: "true",
        },
        "//conditions:default": {},
    })

    # Let user-provided settings have precedence
    config_settings = default | config_settings
    kwargs["config_settings"] = config_settings

def common_executable_macro_kwargs_setup(kwargs):
    convert_legacy_create_init_to_int(kwargs)
    _add_config_setting_defaults(kwargs)

def _transition_executable_impl(settings, attr):
    settings = dict(settings)
    apply_config_settings_attr(settings, attr)

    if attr.python_version and attr.python_version not in ("PY2", "PY3"):
        settings[labels.PYTHON_VERSION] = attr.python_version

    if attr.stamp != -1:
        settings["//command_line_option:stamp"] = str(attr.stamp)

    return settings

def create_executable_rule(*, attrs, **kwargs):
    return create_base_executable_rule(
        attrs = attrs,
        fragments = ["py", "bazel_py"],
        **kwargs
    )

def create_base_executable_rule():
    """Create a function for defining for Python binary/test targets.

    Returns:
        A rule function
    """
    return create_executable_rule_builder().build()

_MaybeBuiltinPyInfo = [BuiltinPyInfo] if BuiltinPyInfo != None else []

# NOTE: Exported publicly
def create_executable_rule_builder(implementation, **kwargs):
    """Create a rule builder for an executable Python program.

    :::{include} /_includes/volatile_api.md
    :::

    An executable rule is one that sets either `executable=True` or `test=True`,
    and the output is something that can be run directly (e.g. `bazel run`,
    `exec(...)` etc)

    :::{versionadded} 1.3.0
    :::

    Returns:
        {obj}`ruleb.Rule` with the necessary settings
        for creating an executable Python rule.
    """
    builder = ruleb.Rule(
        implementation = implementation,
        attrs = EXECUTABLE_ATTRS | (COVERAGE_ATTRS if kwargs.get("test") else {}),
        exec_groups = dict(REQUIRED_EXEC_GROUP_BUILDERS),  # Mutable copy
        fragments = ["py", "bazel_py"],
        provides = [PyExecutableInfo, PyInfo] + _MaybeBuiltinPyInfo,
        toolchains = [
            ruleb.ToolchainType(TOOLCHAIN_TYPE),
            ruleb.ToolchainType(EXEC_TOOLS_TOOLCHAIN_TYPE, mandatory = False),
            ruleb.ToolchainType("@bazel_tools//tools/cpp:toolchain_type", mandatory = False),
        ] + ([ruleb.ToolchainType(LAUNCHER_MAKER_TOOLCHAIN_TYPE)] if rp_config.bazel_9_or_later else []),
        cfg = dict(
            implementation = _transition_executable_impl,
            inputs = TRANSITION_LABELS + [
                labels.PYTHON_VERSION,
                "//command_line_option:stamp",
                "//command_line_option:build_runfile_links",
                "//command_line_option:enable_runfiles",
            ],
            outputs = TRANSITION_LABELS + [
                labels.PYTHON_VERSION,
                "//command_line_option:stamp",
                "//command_line_option:build_runfile_links",
                "//command_line_option:enable_runfiles",
            ],
        ),
        **kwargs
    )
    return builder

def cc_configure_features(
        ctx,
        *,
        cc_toolchain,
        extra_features,
        linking_mode = "static_linking_mode"):
    """Configure C++ features for Python purposes.

    Args:
        ctx: Rule ctx
        cc_toolchain: The CcToolchain the target is using.
        extra_features: list of strings; additional features to request be
            enabled.
        linking_mode: str; either "static_linking_mode" or
            "dynamic_linking_mode". Specifies the linking mode feature for
            C++ linking.

    Returns:
        struct of the feature configuration and all requested features.
    """
    requested_features = [linking_mode]
    requested_features.extend(extra_features)
    requested_features.extend(ctx.features)
    if "legacy_whole_archive" not in ctx.disabled_features:
        requested_features.append("legacy_whole_archive")
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = requested_features,
        unsupported_features = ctx.disabled_features,
    )
    return struct(
        feature_configuration = feature_configuration,
        requested_features = requested_features,
    )
