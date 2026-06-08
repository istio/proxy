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
"""Providers for Python rules."""

DEFAULT_STUB_SHEBANG = "#!/usr/bin/env python3"

_PYTHON_VERSION_VALUES = ["PY2", "PY3"]

def _optional_int(value):
    return int(value) if value != None else None

def interpreter_version_info_struct_from_dict(info_dict):
    """Create a struct of interpreter version info from a dict from an attribute.

    Args:
        info_dict: (dict | None) of version info fields. See interpreter_version_info
            provider field docs.

    Returns:
        struct of version info; see interpreter_version_info provider field docs.
    """
    info_dict = dict(info_dict or {})  # Copy in case the original is frozen
    if info_dict:
        if not ("major" in info_dict and "minor" in info_dict):
            fail("interpreter_version_info must have at least two keys, 'major' and 'minor'")
    version_info_struct = struct(
        major = _optional_int(info_dict.pop("major", None)),
        minor = _optional_int(info_dict.pop("minor", None)),
        micro = _optional_int(info_dict.pop("micro", None)),
        releaselevel = str(info_dict.pop("releaselevel")) if "releaselevel" in info_dict else None,
        serial = _optional_int(info_dict.pop("serial", None)),
    )

    if len(info_dict.keys()) > 0:
        fail("unexpected keys {} in interpreter_version_info".format(
            str(info_dict.keys()),
        ))

    return version_info_struct

def _PyRuntimeInfo_init(
        *,
        implementation_name = None,
        interpreter_path = None,
        interpreter = None,
        files = None,
        coverage_tool = None,
        coverage_files = None,
        pyc_tag = None,
        python_version,
        stub_shebang = None,
        bootstrap_template = None,
        interpreter_version_info = None,
        stage2_bootstrap_template = None,
        zip_main_template = None,
        abi_flags = "",
        site_init_template = None,
        supports_build_time_venv = True):
    if (interpreter_path and interpreter) or (not interpreter_path and not interpreter):
        fail("exactly one of interpreter or interpreter_path must be specified")

    if interpreter_path and files != None:
        fail("cannot specify 'files' if 'interpreter_path' is given")

    if (coverage_tool and not coverage_files) or (not coverage_tool and coverage_files):
        fail(
            "coverage_tool and coverage_files must both be set or neither must be set, " +
            "got coverage_tool={}, coverage_files={}".format(
                coverage_tool,
                coverage_files,
            ),
        )

    if python_version not in _PYTHON_VERSION_VALUES:
        fail("invalid python_version: '{}'; must be one of {}".format(
            python_version,
            _PYTHON_VERSION_VALUES,
        ))

    if files != None and type(files) != type(depset()):
        fail("invalid files: got value of type {}, want depset".format(type(files)))

    if interpreter:
        if files == None:
            files = depset()
    else:
        files = None

    if coverage_files == None:
        coverage_files = depset()

    if not stub_shebang:
        stub_shebang = DEFAULT_STUB_SHEBANG

    return {
        "abi_flags": abi_flags,
        "bootstrap_template": bootstrap_template,
        "coverage_files": coverage_files,
        "coverage_tool": coverage_tool,
        "files": files,
        "implementation_name": implementation_name,
        "interpreter": interpreter,
        "interpreter_path": interpreter_path,
        "interpreter_version_info": interpreter_version_info_struct_from_dict(interpreter_version_info),
        "pyc_tag": pyc_tag,
        "python_version": python_version,
        "site_init_template": site_init_template,
        "stage2_bootstrap_template": stage2_bootstrap_template,
        "stub_shebang": stub_shebang,
        "supports_build_time_venv": supports_build_time_venv,
        "zip_main_template": zip_main_template,
    }

PyRuntimeInfo, _unused_raw_py_runtime_info_ctor = provider(
    doc = """Contains information about a Python runtime, as returned by the `py_runtime`
rule.

:::{warning}
This is an **unstable public** API. It may change more frequently and has weaker
compatibility guarantees.
:::

A Python runtime describes either a *platform runtime* or an *in-build runtime*.
A platform runtime accesses a system-installed interpreter at a known path,
whereas an in-build runtime points to a `File` that acts as the interpreter. In
both cases, an "interpreter" is really any executable binary or wrapper script
that is capable of running a Python script passed on the command line, following
the same conventions as the standard CPython interpreter.
""",
    init = _PyRuntimeInfo_init,
    fields = {
        "abi_flags": """
:type: str

The runtime's ABI flags, i.e. `sys.abiflags`.

:::{versionadded} 1.0.0
:::
""",
        "bootstrap_template": """
:type: File

A template of code responsible for the initial startup of a program.

This code is responsible for:

* Locating the target interpreter. Typically it is in runfiles, but not always.
* Setting necessary environment variables, command line flags, or other
  configuration that can't be modified after the interpreter starts.
* Invoking the appropriate entry point. This is usually a second-stage bootstrap
  that performs additional setup prior to running a program's actual entry point.

The {obj}`--bootstrap_impl` flag affects how this stage 1 bootstrap
is expected to behave and the substutitions performed.

* `--bootstrap_impl=system_python` substitutions: `%is_zipfile%`, `%python_binary%`,
  `%target%`, `%workspace_name`, `%coverage_tool%`, `%import_all%`, `%imports%`,
  `%main%`, `%shebang%`
* `--bootstrap_impl=script` substititions: `%is_zipfile%`, `%python_binary%`,
  `%python_binary_actual%`, `%target%`, `%workspace_name`,
  `%shebang%`, `%stage2_bootstrap%`

Substitution definitions:

* `%shebang%`: The shebang to use with the bootstrap; the bootstrap template
  may choose to ignore this.
* `%stage2_bootstrap%`: A runfiles-relative path to the stage 2 bootstrap.
* `%python_binary%`: The path to the target Python interpreter. There are three
  types of paths:
  * An absolute path to a system interpreter (e.g. begins with `/`).
  * A runfiles-relative path to an interpreter (e.g. `somerepo/bin/python3`)
  * A program to search for on PATH, i.e. a word without spaces, e.g. `python3`.

  When `--bootstrap_impl=script` is used, this is always a runfiles-relative
  path to a venv-based interpreter executable.

* `%python_binary_actual%`: The path to the interpreter that
  `%python_binary%` invokes. There are three types of paths:
  * An absolute path to a system interpreter (e.g. begins with `/`).
  * A runfiles-relative path to an interpreter (e.g. `somerepo/bin/python3`)
  * A program to search for on PATH, i.e. a word without spaces, e.g. `python3`.

  Only set for zip builds with `--bootstrap_impl=script`; other builds will use
  an empty string.

* `%workspace_name%`: The name of the workspace the target belongs to.
* `%is_zipfile%`: The string `1` if this template is prepended to a zipfile to
  create a self-executable zip file. The string `0` otherwise.

For the other substitution definitions, see the {obj}`stage2_bootstrap_template`
docs.

:::{versionchanged} 0.33.0
The set of substitutions depends on {obj}`--bootstrap_impl`
:::
""",
        "coverage_files": """
:type: depset[File] | None

The files required at runtime for using `coverage_tool`. Will be `None` if no
`coverage_tool` was provided.
""",
        "coverage_tool": """
:type: File | None

If set, this field is a `File` representing tool used for collecting code
coverage information from python tests. Otherwise, this is `None`.
""",
        "files": """
:type: depset[File] | None

If this is an in-build runtime, this field is a `depset` of `File`s that need to
be added to the runfiles of an executable target that uses this runtime (in
particular, files needed by `interpreter`). The value of `interpreter` need not
be included in this field. If this is a platform runtime then this field is
`None`.
""",
        "implementation_name": """
:type: str | None

The Python implementation name (`sys.implementation.name`)
""",
        "interpreter": """
:type: File | None

If this is an in-build runtime, this field is a `File` representing the
interpreter. Otherwise, this is `None`. Note that an in-build runtime can use
either a prebuilt, checked-in interpreter or an interpreter built from source.
""",
        "interpreter_path": """
:type: str | None

If this is a platform runtime, this field is the absolute filesystem path to the
interpreter on the target platform. Otherwise, this is `None`.
""",
        "interpreter_version_info": """
:type: struct

Version information about the interpreter this runtime provides.
It should match the format given by `sys.version_info`, however
for simplicity, the micro, releaselevel, and serial values are
optional.
A struct with the following fields:
* `major`: {type}`int`, the major version number
* `minor`: {type}`int`, the minor version number
* `micro`: {type}`int | None`, the micro version number
* `releaselevel`: {type}`str | None`, the release level
* `serial`: {type}`int | None`, the serial number of the release
""",
        "pyc_tag": """
:type: str | None

The tag portion of a pyc filename, e.g. the `cpython-39` infix
of `foo.cpython-39.pyc`. See PEP 3147. If not specified, it will be computed
from {obj}`implementation_name` and {obj}`interpreter_version_info`. If no
pyc_tag is available, then only source-less pyc generation will function
correctly.
""",
        "python_version": """
:type: str

Indicates whether this runtime uses Python major version 2 or 3. Valid values
are (only) `"PY2"` and `"PY3"`.
""",
        "site_init_template": """
:type: File

The template to use for the binary-specific site-init hook run by the
interpreter at startup.

:::{versionadded} 1.0.0
:::
""",
        "stage2_bootstrap_template": """
:type: File

A template of Python code that runs under the desired interpreter and is
responsible for orchestrating calling the program's actual main code. This
bootstrap is responsible for affecting the current runtime's state, such as
import paths or enabling coverage, so that, when it runs the program's actual
main code, it works properly under Bazel.

The following substitutions are made during template expansion:
* `%main%`: A runfiles-relative path to the program's actual main file. This
  can be a `.py` or `.pyc` file, depending on precompile settings.
* `%coverage_tool%`: Runfiles-relative path to the coverage library's entry point.
  If coverage is not enabled or available, an empty string.
* `%import_all%`: The string `True` if all repositories in the runfiles should
  be added to sys.path. The string `False` otherwise.
* `%imports%`: A colon-delimited string of runfiles-relative paths to add to
  sys.path.
* `%target%`: The name of the target this is for.
* `%workspace_name%`: The name of the workspace the target belongs to.

:::{versionadded} 0.33.0
:::
""",
        "stub_shebang": """
:type: str

"Shebang" expression prepended to the bootstrapping Python stub
script used when executing {obj}`py_binary` targets.  Does not
apply to Windows.
""",
        "supports_build_time_venv": """
:type: bool

True if this toolchain supports the build-time created virtual environment.
False if not or unknown. If build-time venv creation isn't supported, then binaries may
fallback to non-venv solutions or creating a venv at runtime.

In order to use the build-time created virtual environment, a toolchain needs
to meet two criteria:
1. Specifying the underlying executable (e.g. `/usr/bin/python3`, as reported by
   `sys._base_executable`) for the venv executable (`$venv/bin/python3`, as reported
   by `sys.executable`). This typically requires relative symlinking the venv
   path to the underlying path at build time, or using the `PYTHONEXECUTABLE`
   environment variable (Python 3.11+) at runtime.
2. Having the build-time created site-packages directory
   (`<venv>/lib/python{version}/site-packages`) recognized by the runtime
   interpreter. This typically requires the Python version to be known at
   build-time and match at runtime.

:::{versionadded} 1.5.0
:::
""",
        "zip_main_template": """
:type: File

A template of Python code that becomes a zip file's top-level `__main__.py`
file. The top-level `__main__.py` file is used when the zip file is explicitly
passed to a Python interpreter. See PEP 441 for more information about zipapp
support. Note that py_binary-generated zip files are self-executing and
skip calling `__main__.py`.

The following substitutions are made during template expansion:
* `%stage2_bootstrap%`: A runfiles-relative string to the stage 2 bootstrap file.
* `%python_binary%`: The path to the target Python interpreter. There are three
  types of paths:
  * An absolute path to a system interpreter (e.g. begins with `/`).
  * A runfiles-relative path to an interpreter (e.g. `somerepo/bin/python3`)
  * A program to search for on PATH, i.e. a word without spaces, e.g. `python3`.
* `%workspace_name%`: The name of the workspace for the built target.

:::{versionadded} 0.33.0
:::
""",
    },
)
