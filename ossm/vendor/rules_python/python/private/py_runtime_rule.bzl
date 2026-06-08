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
"""Implementation of py_runtime rule."""

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load(":common_labels.bzl", "labels")
load(":flags.bzl", "FreeThreadedFlag")
load(":py_internal.bzl", "py_internal")
load(":py_runtime_info.bzl", "DEFAULT_STUB_SHEBANG", "PyRuntimeInfo")
load(":reexports.bzl", "BuiltinPyRuntimeInfo")
load(":version.bzl", "version")

_py_builtins = py_internal

def _py_runtime_impl(ctx):
    interpreter_path = ctx.attr.interpreter_path or None  # Convert empty string to None
    interpreter = ctx.attr.interpreter
    if (interpreter_path and interpreter) or (not interpreter_path and not interpreter):
        fail("exactly one of the 'interpreter' or 'interpreter_path' attributes must be specified")

    runtime_files = depset(transitive = [
        t[DefaultInfo].files
        for t in ctx.attr.files
    ])

    runfiles = ctx.runfiles()

    hermetic = bool(interpreter)
    if not hermetic:
        if runtime_files:
            fail("if 'interpreter_path' is given then 'files' must be empty")
        if not paths.is_absolute(interpreter_path):
            fail("interpreter_path must be an absolute path")
    else:
        interpreter_di = interpreter[DefaultInfo]

        if interpreter_di.files_to_run and interpreter_di.files_to_run.executable:
            interpreter = interpreter_di.files_to_run.executable
            runfiles = runfiles.merge(interpreter_di.default_runfiles)

            runtime_files = depset(transitive = [
                interpreter_di.files,
                interpreter_di.default_runfiles.files,
                runtime_files,
            ])
        elif _is_singleton_depset(interpreter_di.files):
            interpreter = interpreter_di.files.to_list()[0]
        else:
            fail("interpreter must be an executable target or must produce exactly one file.")

    if ctx.attr.coverage_tool:
        coverage_di = ctx.attr.coverage_tool[DefaultInfo]

        if _is_singleton_depset(coverage_di.files):
            coverage_tool = coverage_di.files.to_list()[0]
        elif coverage_di.files_to_run and coverage_di.files_to_run.executable:
            coverage_tool = coverage_di.files_to_run.executable
        else:
            fail("coverage_tool must be an executable target or must produce exactly one file.")

        coverage_files = depset(transitive = [
            coverage_di.files,
            coverage_di.default_runfiles.files,
        ])
    else:
        coverage_tool = None
        coverage_files = None

    python_version = ctx.attr.python_version

    interpreter_version_info = ctx.attr.interpreter_version_info
    if not interpreter_version_info:
        python_version_flag = ctx.attr._python_version_flag[BuildSettingInfo].value
        if python_version_flag:
            interpreter_version_info = _interpreter_version_info_from_version_str(python_version_flag)

    if python_version == "PY2":
        fail("Using Python 2 is not supported and disabled; see " +
             "https://github.com/bazelbuild/bazel/issues/15684")

    pyc_tag = ctx.attr.pyc_tag
    if not pyc_tag and (ctx.attr.implementation_name and
                        interpreter_version_info.get("major") and
                        interpreter_version_info.get("minor")):
        pyc_tag = "{}-{}{}".format(
            ctx.attr.implementation_name,
            interpreter_version_info["major"],
            interpreter_version_info["minor"],
        )

    abi_flags = ctx.attr.abi_flags
    if abi_flags == "<AUTO>":
        abi_flags = ""
        if ctx.attr._py_freethreaded_flag[BuildSettingInfo].value == FreeThreadedFlag.YES:
            abi_flags += "t"

    # Args common to both BuiltinPyRuntimeInfo and PyRuntimeInfo
    py_runtime_info_kwargs = dict(
        interpreter_path = interpreter_path or None,
        interpreter = interpreter,
        files = runtime_files if hermetic else None,
        coverage_tool = coverage_tool,
        coverage_files = coverage_files,
        python_version = python_version,
        stub_shebang = ctx.attr.stub_shebang,
        bootstrap_template = ctx.file.bootstrap_template,
    )
    builtin_py_runtime_info_kwargs = dict(py_runtime_info_kwargs)

    # There are all args that BuiltinPyRuntimeInfo doesn't support
    py_runtime_info_kwargs.update(dict(
        implementation_name = ctx.attr.implementation_name,
        interpreter_version_info = interpreter_version_info,
        pyc_tag = pyc_tag,
        stage2_bootstrap_template = ctx.file.stage2_bootstrap_template,
        zip_main_template = ctx.file.zip_main_template,
        abi_flags = abi_flags,
        site_init_template = ctx.file.site_init_template,
        supports_build_time_venv = ctx.attr.supports_build_time_venv,
    ))

    providers = [
        PyRuntimeInfo(**py_runtime_info_kwargs),
        DefaultInfo(
            files = runtime_files,
            runfiles = runfiles,
        ),
    ]
    if BuiltinPyRuntimeInfo != None and BuiltinPyRuntimeInfo != PyRuntimeInfo:
        # Return the builtin provider for better compatibility.
        # 1. There is a legacy code path in py_binary that
        #    checks for the provider when toolchains aren't used
        # 2. It makes it easier to transition from builtins to rules_python
        providers.append(BuiltinPyRuntimeInfo(**builtin_py_runtime_info_kwargs))
    return providers

# Bind to the name "py_runtime" to preserve the kind/rule_class it shows up
# as elsewhere.
py_runtime = rule(
    implementation = _py_runtime_impl,
    doc = """
Represents a Python runtime used to execute Python code.

A `py_runtime` target can represent either a *platform runtime* or an *in-build
runtime*. A platform runtime accesses a system-installed interpreter at a known
path, whereas an in-build runtime points to an executable target that acts as
the interpreter. In both cases, an "interpreter" means any executable binary or
wrapper script that is capable of running a Python script passed on the command
line, following the same conventions as the standard CPython interpreter.

A platform runtime is by its nature non-hermetic. It imposes a requirement on
the target platform to have an interpreter located at a specific path. An
in-build runtime may or may not be hermetic, depending on whether it points to
a checked-in interpreter or a wrapper script that accesses the system
interpreter.

Example

```
load("@rules_python//python:py_runtime.bzl", "py_runtime")

py_runtime(
    name = "python-2.7.12",
    files = glob(["python-2.7.12/**"]),
    interpreter = "python-2.7.12/bin/python",
)

py_runtime(
    name = "python-3.6.0",
    interpreter_path = "/opt/pyenv/versions/3.6.0/bin/python",
)
```
""",
    fragments = ["py"],
    attrs = dicts.add(
        {
            "abi_flags": attr.string(
                default = "<AUTO>",
                doc = """
The runtime's ABI flags, i.e. `sys.abiflags`.

If not set, then it will be set based on flags.
""",
            ),
            "bootstrap_template": attr.label(
                allow_single_file = True,
                default = Label("//python/private:bootstrap_template"),
                doc = """
The bootstrap script template file to use. Should have %python_binary%,
%workspace_name%, %main%, and %imports%.

This template, after expansion, becomes the executable file used to start the
process, so it is responsible for initial bootstrapping actions such as finding
the Python interpreter, runfiles, and constructing an environment to run the
intended Python application.

While this attribute is currently optional, it will become required when the
Python rules are moved out of Bazel itself.

The exact variable names expanded is an unstable API and is subject to change.
The API will become more stable when the Python rules are moved out of Bazel
itself.

See @bazel_tools//tools/python:python_bootstrap_template.txt for more variables.
""",
            ),
            "coverage_tool": attr.label(
                allow_files = False,
                doc = """
This is a target to use for collecting code coverage information from
{rule}`py_binary` and {rule}`py_test` targets.

If set, the target must either produce a single file or be an executable target.
The path to the single file, or the executable if the target is executable,
determines the entry point for the python coverage tool.  The target and its
runfiles will be added to the runfiles when coverage is enabled.

The entry point for the tool must be loadable by a Python interpreter (e.g. a
`.py` or `.pyc` file).  It must accept the command line arguments
of [`coverage.py`](https://coverage.readthedocs.io), at least including
the `run` and `lcov` subcommands.
""",
            ),
            "files": attr.label_list(
                allow_files = True,
                doc = """
For an in-build runtime, this is the set of files comprising this runtime.
These files will be added to the runfiles of Python binaries that use this
runtime. For a platform runtime this attribute must not be set.
""",
            ),
            "implementation_name": attr.string(
                doc = "The Python implementation name (`sys.implementation.name`)",
                default = "cpython",
            ),
            "interpreter": attr.label(
                # We set `allow_files = True` to allow specifying executable
                # targets from rules that have more than one default output,
                # e.g. sh_binary.
                allow_files = True,
                doc = """
For an in-build runtime, this is the target to invoke as the interpreter. It
can be either of:

* A single file, which will be the interpreter binary. It's assumed such
  interpreters are either self-contained single-file executables or any
  supporting files are specified in `files`.
* An executable target. The target's executable will be the interpreter binary.
  Any other default outputs (`target.files`) and plain files runfiles
  (`runfiles.files`) will be automatically included as if specified in the
  `files` attribute.

  NOTE: the runfiles of the target may not yet be properly respected/propagated
  to consumers of the toolchain/interpreter, see
  bazel-contrib/rules_python/issues/1612

For a platform runtime (i.e. `interpreter_path` being set) this attribute must
not be set.
""",
            ),
            "interpreter_path": attr.string(doc = """
For a platform runtime, this is the absolute path of a Python interpreter on
the target platform. For an in-build runtime this attribute must not be set.
"""),
            "interpreter_version_info": attr.string_dict(
                doc = """
Version information about the interpreter this runtime provides.

If not specified, uses {obj}`--python_version`

The supported keys match the names for `sys.version_info`. While the input
values are strings, most are converted to ints. The supported keys are:
  * major: int, the major version number
  * minor: int, the minor version number
  * micro: optional int, the micro version number
  * releaselevel: optional str, the release level
  * serial: optional int, the serial number of the release

:::{versionchanged} 0.36.0
{obj}`--python_version` determines the default value.
:::
""",
                mandatory = False,
            ),
            "pyc_tag": attr.string(
                doc = """
Optional string; the tag portion of a pyc filename, e.g. the `cpython-39` infix
of `foo.cpython-39.pyc`. See PEP 3147. If not specified, it will be computed
from `implementation_name` and `interpreter_version_info`. If no pyc_tag is
available, then only source-less pyc generation will function correctly.
""",
            ),
            "python_version": attr.string(
                default = "PY3",
                values = ["PY2", "PY3"],
                doc = """
Whether this runtime is for Python major version 2 or 3. Valid values are `"PY2"`
and `"PY3"`.

The default value is controlled by the `--incompatible_py3_is_default` flag.
However, in the future this attribute will be mandatory and have no default
value.
            """,
            ),
            "site_init_template": attr.label(
                allow_single_file = True,
                default = "//python/private:site_init_template",
                doc = """
The template to use for the binary-specific site-init hook run by the
interpreter at startup.

:::{versionadded} 0.41.0
:::
""",
            ),
            "stage2_bootstrap_template": attr.label(
                default = "//python/private:stage2_bootstrap_template",
                allow_single_file = True,
                doc = """
The template to use when two stage bootstrapping is enabled

:::{seealso}
{obj}`PyRuntimeInfo.stage2_bootstrap_template` and {obj}`--bootstrap_impl`
:::
""",
            ),
            "stub_shebang": attr.string(
                default = DEFAULT_STUB_SHEBANG,
                doc = """
"Shebang" expression prepended to the bootstrapping Python stub script
used when executing {rule}`py_binary` targets.

See https://github.com/bazelbuild/bazel/issues/8685 for
motivation.

Does not apply to Windows.
""",
            ),
            "supports_build_time_venv": attr.bool(
                doc = """
Whether this runtime supports virtualenvs created at build time.

See {obj}`PyRuntimeInfo.supports_build_time_venv` for docs.

:::{versionadded} 1.5.0
:::
""",
                default = True,
            ),
            "zip_main_template": attr.label(
                default = "//python/private/zipapp:zip_main_template",
                allow_single_file = True,
                doc = """
The template to use for a zip's top-level `__main__.py` file.

This becomes the entry point executed when `python foo.zip` is run.

:::{seealso}
The {obj}`PyRuntimeInfo.zip_main_template` field.
:::
""",
            ),
            "_py_freethreaded_flag": attr.label(
                default = labels.PY_FREETHREADED,
            ),
            "_python_version_flag": attr.label(
                default = labels.PYTHON_VERSION,
            ),
        },
    ),
)

def _is_singleton_depset(files):
    return _py_builtins.is_singleton_depset(files)

def _interpreter_version_info_from_version_str(version_str):
    v = version.parse(version_str)
    version_info = {}
    parts = list(v.release)
    for key in ("major", "minor", "micro"):
        if not parts:
            break
        version_info[key] = parts.pop(0)
    return version_info
