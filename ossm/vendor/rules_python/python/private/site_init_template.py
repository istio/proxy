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
"""site initialization logic for Bazel-built py_binary targets."""
import os
import os.path
import sys

# Colon-delimited string of runfiles-relative import paths to add
_IMPORTS_STR = "%imports%"
# Though the import all value is the correct literal, we quote it
# so this file is parsable by tools.
_IMPORT_ALL = "%import_all%" == "True"
_WORKSPACE_NAME = "%workspace_name%"
# runfiles-relative path to this file
_SELF_RUNFILES_RELATIVE_PATH = "%site_init_runfiles_path%"
# Runfiles-relative path to the coverage tool entry point, if any.
_COVERAGE_TOOL = "%coverage_tool%"
# True if the runfiles root should be added to sys.path
_ADD_RUNFILES_ROOT_TO_SYS_PATH = "%add_runfiles_root_to_sys_path%" == "1"


def _is_verbose():
    return bool(os.environ.get("RULES_PYTHON_BOOTSTRAP_VERBOSE"))


def _print_verbose_coverage(*args):
    if os.environ.get("VERBOSE_COVERAGE") or _is_verbose():
        _print_verbose(*args)


def _print_verbose(*args, mapping=None, values=None):
    if not _is_verbose():
        return

    print("bazel_site_init:", *args, file=sys.stderr, flush=True)


_print_verbose("imports_str:", _IMPORTS_STR)
_print_verbose("import_all:", _IMPORT_ALL)
_print_verbose("workspace_name:", _WORKSPACE_NAME)
_print_verbose("self_runfiles_path:", _SELF_RUNFILES_RELATIVE_PATH)
_print_verbose("coverage_tool:", _COVERAGE_TOOL)


def _find_runfiles_root():
    # Give preference to the environment variables
    runfiles_dir = os.environ.get("RUNFILES_DIR", None)
    if not runfiles_dir:
        runfiles_manifest_file = os.environ.get("RUNFILES_MANIFEST_FILE", "")
        if runfiles_manifest_file.endswith(
            ".runfiles_manifest"
        ) or runfiles_manifest_file.endswith(".runfiles/MANIFEST"):
            runfiles_dir = runfiles_manifest_file[:-9]

    # Be defensive: the runfiles dir should contain ourselves. If it doesn't,
    # then it must not be our runfiles directory.
    if runfiles_dir and os.path.exists(
        os.path.join(runfiles_dir, _SELF_RUNFILES_RELATIVE_PATH)
    ):
        return runfiles_dir

    num_dirs_to_runfiles_root = _SELF_RUNFILES_RELATIVE_PATH.count("/") + 1
    runfiles_root = os.path.dirname(__file__)
    for _ in range(num_dirs_to_runfiles_root):
        runfiles_root = os.path.dirname(runfiles_root)
    return runfiles_root


_RUNFILES_ROOT = _find_runfiles_root()

_print_verbose("runfiles_root:", _RUNFILES_ROOT)


def _is_windows():
    return os.name == "nt"


def _get_windows_path_with_unc_prefix(path):
    path = path.strip()
    # No need to add prefix for non-Windows platforms.
    if not _is_windows() or sys.version_info[0] < 3:
        return path

    # Starting in Windows 10, version 1607(OS build 14393), MAX_PATH limitations have been
    # removed from common Win32 file and directory functions.
    # Related doc: https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=cmd#enable-long-paths-in-windows-10-version-1607-and-later
    import platform

    if platform.win32_ver()[1] >= "10.0.14393":
        return path

    # import sysconfig only now to maintain python 2.6 compatibility
    import sysconfig

    if sysconfig.get_platform() == "mingw":
        return path

    # Lets start the unicode fun
    unicode_prefix = "\\\\?\\"
    if path.startswith(unicode_prefix):
        return path

    # os.path.abspath returns a normalized absolute path
    return unicode_prefix + os.path.abspath(path)


def _search_path(name):
    """Finds a file in a given search path."""
    search_path = os.getenv("PATH", os.defpath).split(os.pathsep)
    for directory in search_path:
        if directory:
            path = os.path.join(directory, name)
            if os.path.isfile(path) and os.access(path, os.X_OK):
                return path
    return None


def _setup_sys_path():
    """Perform Bazel/binary specific sys.path setup.

    """
    seen = set(sys.path)
    python_path_entries = []

    def _maybe_add_path(path):
        if path in seen:
            return
        path = _get_windows_path_with_unc_prefix(path)
        if _is_windows():
            path = path.replace("/", os.sep)

        _print_verbose("append sys.path:", path)
        sys.path.append(path)
        seen.add(path)

    # Adding the runfiles root to sys.path is a legacy behavior that will be
    # removed. We don't want to add it to sys.path for two reasons:
    # 1. Under workspace, it makes every external repository importable. If a Bazel
    #    repository matches a Python import name, they conflict.
    # 2. Under bzlmod, the repo names in the runfiles directory aren't importable
    #    Python names, so there's no point in adding the runfiles root to sys.path.
    # For temporary compatibility with the original system_python bootstrap
    # behavior, it is conditionally added for that boostrap mode.
    if _ADD_RUNFILES_ROOT_TO_SYS_PATH:
        _maybe_add_path(_RUNFILES_ROOT)

    for rel_path in _IMPORTS_STR.split(":"):
        abs_path = os.path.join(_RUNFILES_ROOT, rel_path)
        _maybe_add_path(abs_path)

    if _IMPORT_ALL:
        repo_dirs = sorted(
            os.path.join(_RUNFILES_ROOT, d) for d in os.listdir(_RUNFILES_ROOT)
        )
        for d in repo_dirs:
            if os.path.isdir(d):
                _maybe_add_path(d)
    else:
        _maybe_add_path(os.path.join(_RUNFILES_ROOT, _WORKSPACE_NAME))

    # COVERAGE_DIR is set if coverage is enabled and instrumentation is configured
    # for something, though it could be another program executing this one or
    # one executed by this one (e.g. an extension module).
    # NOTE: Coverage is added last to allow user dependencies to override it.
    coverage_setup = False
    if os.environ.get("COVERAGE_DIR"):
        cov_tool = _COVERAGE_TOOL
        if cov_tool:
            _print_verbose_coverage(f"Using toolchain coverage_tool {cov_tool}")
        elif cov_tool := os.environ.get("PYTHON_COVERAGE"):
            _print_verbose_coverage(
                f"Using env var coverage: PYTHON_COVERAGE={cov_tool}"
            )

        if cov_tool:
            if os.path.isabs(cov_tool):
                pass
            elif os.sep in os.path.normpath(cov_tool):
                cov_tool = os.path.join(_RUNFILES_ROOT, cov_tool)
            else:
                cov_tool = _search_path(cov_tool)
        if cov_tool:
            # The coverage entry point is `<dir>/coverage/coverage_main.py`, so
            # we need to do twice the dirname so that `import coverage` works
            coverage_dir = os.path.dirname(os.path.dirname(cov_tool))

            # coverage library expects sys.path[0] to contain the library, and replaces
            # it with the directory of the program it starts. Our actual sys.path[0] is
            # the runfiles directory, which must not be replaced.
            # CoverageScript.do_execute() undoes this sys.path[0] setting.
            _maybe_add_path(coverage_dir)
            coverage_setup = True
        else:
            _print_verbose_coverage(
                "Coverage was enabled, but the coverage tool was not found or valid. "
                + "To enable coverage, consult the docs at "
                + "https://rules-python.readthedocs.io/en/latest/coverage.html"
            )

    return coverage_setup


def _fixup_sys_base_executable():
    """Fixup sys._base_executable to account for Bazel-specific pyvenv.cfg

    The pyvenv.cfg created for py_binary leaves the `home` key unset. A
    side-effect of this is `sys._base_executable` points to the venv executable,
    not the actual executable. This mostly doesn't matter, but does affect
    using the venv module to create venvs (they point to the venv executable, not
    the actual executable).
    """
    # Must have been set correctly?
    if sys.executable != sys._base_executable:
        return
    # Not in a venv, so don't touch anything.
    if sys.prefix == sys.base_prefix:
        return
    exe = os.path.realpath(sys.executable)
    _print_verbose("setting sys._base_executable:", exe)
    sys._base_executable = exe


_fixup_sys_base_executable()

COVERAGE_SETUP = _setup_sys_path()
_print_verbose("DONE")
