# Template for the __main__.py file inserted into zip files
#
# NOTE: This file is a "stage 1" bootstrap, so it's responsible for locating the
# desired runtime and having it run the stage 2 bootstrap. This means it can't
# assume much about the current runtime and environment. e.g., the current
# runtime may not be the correct one, the zip may not have been extract, the
# runfiles env vars may not be set, etc.
#
# NOTE: This program must retain compatibility with a wide variety of Python
# versions since it is run by an unknown Python interpreter.

import sys

# The Python interpreter unconditionally prepends the directory containing this
# script (following symlinks) to the import path. This is the cause of #9239,
# and is a special case of #7091. We therefore explicitly delete that entry.
# TODO(#7091): Remove this hack when no longer necessary.
del sys.path[0]

import os
import shutil
import subprocess
import tempfile
import zipfile

# runfiles-relative path
_STAGE2_BOOTSTRAP = "%stage2_bootstrap%"
# runfiles-relative path
_PYTHON_BINARY = "%python_binary%"
# runfiles-relative path, absolute path, or single word
_PYTHON_BINARY_ACTUAL = "%python_binary_actual%"
_WORKSPACE_NAME = "%workspace_name%"


# Return True if running on Windows
def is_windows():
    return os.name == "nt"


def get_windows_path_with_unc_prefix(path):
    """Adds UNC prefix after getting a normalized absolute Windows path.

    No-op for non-Windows platforms or if running under python2.
    """
    path = path.strip()

    # No need to add prefix for non-Windows platforms.
    # And \\?\ doesn't work in python 2 or on mingw
    if not is_windows() or sys.version_info[0] < 3:
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


def has_windows_executable_extension(path):
    return path.endswith(".exe") or path.endswith(".com") or path.endswith(".bat")


if is_windows() and not has_windows_executable_extension(_PYTHON_BINARY):
    _PYTHON_BINARY = _PYTHON_BINARY + ".exe"


def search_path(name):
    """Finds a file in a given search path."""
    search_path = os.getenv("PATH", os.defpath).split(os.pathsep)
    for directory in search_path:
        if directory:
            path = os.path.join(directory, name)
            if os.path.isfile(path) and os.access(path, os.X_OK):
                return path
    return None


def find_python_binary(module_space):
    """Finds the real Python binary if it's not a normal absolute path."""
    return find_binary(module_space, _PYTHON_BINARY)


def print_verbose(*args, mapping=None, values=None):
    if bool(os.environ.get("RULES_PYTHON_BOOTSTRAP_VERBOSE")):
        if mapping is not None:
            for key, value in sorted((mapping or {}).items()):
                print(
                    "bootstrap: stage 1:",
                    *args,
                    f"{key}={value!r}",
                    file=sys.stderr,
                    flush=True,
                )
        elif values is not None:
            for i, v in enumerate(values):
                print(
                    "bootstrap: stage 1:",
                    *args,
                    f"[{i}] {v!r}",
                    file=sys.stderr,
                    flush=True,
                )
        else:
            print("bootstrap: stage 1:", *args, file=sys.stderr, flush=True)


def find_binary(module_space, bin_name):
    """Finds the real binary if it's not a normal absolute path."""
    if not bin_name:
        return None
    if bin_name.startswith("//"):
        # Case 1: Path is a label. Not supported yet.
        raise AssertionError(
            "Bazel does not support execution of Python interpreters via labels yet"
        )
    elif os.path.isabs(bin_name):
        # Case 2: Absolute path.
        return bin_name
    # Use normpath() to convert slashes to os.sep on Windows.
    elif os.sep in os.path.normpath(bin_name):
        # Case 3: Path is relative to the repo root.
        return os.path.join(module_space, bin_name)
    else:
        # Case 4: Path has to be looked up in the search path.
        return search_path(bin_name)


def extract_zip(zip_path, dest_dir):
    """Extracts the contents of a zip file, preserving the unix file mode bits.

    These include the permission bits, and in particular, the executable bit.

    Ideally the zipfile module should set these bits, but it doesn't. See:
    https://bugs.python.org/issue15795.

    Args:
        zip_path: The path to the zip file to extract
        dest_dir: The path to the destination directory
    """
    zip_path = get_windows_path_with_unc_prefix(zip_path)
    dest_dir = get_windows_path_with_unc_prefix(dest_dir)
    with zipfile.ZipFile(zip_path) as zf:
        for info in zf.infolist():
            zf.extract(info, dest_dir)
            # UNC-prefixed paths must be absolute/normalized. See
            # https://docs.microsoft.com/en-us/windows/desktop/fileio/naming-a-file#maximum-path-length-limitation
            file_path = os.path.abspath(os.path.join(dest_dir, info.filename))
            # The Unix st_mode bits (see "man 7 inode") are stored in the upper 16
            # bits of external_attr. Of those, we set the lower 12 bits, which are the
            # file mode bits (since the file type bits can't be set by chmod anyway).
            attrs = info.external_attr >> 16
            if attrs != 0:  # Rumor has it these can be 0 for zips created on Windows.
                os.chmod(file_path, attrs & 0o7777)


# Create the runfiles tree by extracting the zip file
def create_module_space():
    temp_dir = tempfile.mkdtemp("", "Bazel.runfiles_")
    extract_zip(os.path.dirname(__file__), temp_dir)
    # IMPORTANT: Later code does `rm -fr` on dirname(module_space) -- it's
    # important that deletion code be in sync with this directory structure
    return os.path.join(temp_dir, "runfiles")


def execute_file(
    python_program,
    main_filename,
    args,
    env,
    module_space,
    workspace,
):
    # type: (str, str, list[str], dict[str, str], str, str|None, str|None) -> ...
    """Executes the given Python file using the various environment settings.

    This will not return, and acts much like os.execv, except is much
    more restricted, and handles Bazel-related edge cases.

    Args:
      python_program: (str) Path to the Python binary to use for execution
      main_filename: (str) The Python file to execute
      args: (list[str]) Additional args to pass to the Python file
      env: (dict[str, str]) A dict of environment variables to set for the execution
      module_space: (str) Path to the module space/runfiles tree directory
      workspace: (str|None) Name of the workspace to execute in. This is expected to be a
          directory under the runfiles tree.
    """
    # We want to use os.execv instead of subprocess.call, which causes
    # problems with signal passing (making it difficult to kill
    # Bazel). However, these conditions force us to run via
    # subprocess.call instead:
    #
    # - On Windows, os.execv doesn't handle arguments with spaces
    #   correctly, and it actually starts a subprocess just like
    #   subprocess.call.
    # - When running in a workspace or zip file, we need to clean up the
    #   workspace after the process finishes so control must return here.
    try:
        subprocess_argv = [python_program, main_filename] + args
        print_verbose("subprocess argv:", values=subprocess_argv)
        print_verbose("subprocess env:", mapping=env)
        print_verbose("subprocess cwd:", workspace)
        ret_code = subprocess.call(subprocess_argv, env=env, cwd=workspace)
        sys.exit(ret_code)
    finally:
        # NOTE: dirname() is called because create_module_space() creates a
        # sub-directory within a temporary directory, and we want to remove the
        # whole temporary directory.
        shutil.rmtree(os.path.dirname(module_space), True)


def main():
    print_verbose("running zip main bootstrap")
    print_verbose("initial argv:", values=sys.argv)
    print_verbose("initial environ:", mapping=os.environ)
    print_verbose("initial sys.executable", sys.executable)
    print_verbose("initial sys.version", sys.version)

    args = sys.argv[1:]

    new_env = {}

    # The main Python source file.
    # The magic string percent-main-percent is replaced with the runfiles-relative
    # filename of the main file of the Python binary in BazelPythonSemantics.java.
    main_rel_path = _STAGE2_BOOTSTRAP
    if is_windows():
        main_rel_path = main_rel_path.replace("/", os.sep)

    module_space = create_module_space()
    print_verbose("extracted runfiles to:", module_space)

    new_env["RUNFILES_DIR"] = module_space

    # Don't prepend a potentially unsafe path to sys.path
    # See: https://docs.python.org/3.11/using/cmdline.html#envvar-PYTHONSAFEPATH
    new_env["PYTHONSAFEPATH"] = "1"

    main_filename = os.path.join(module_space, main_rel_path)
    main_filename = get_windows_path_with_unc_prefix(main_filename)
    assert os.path.exists(main_filename), (
        "Cannot exec() %r: file not found." % main_filename
    )
    assert os.access(main_filename, os.R_OK), (
        "Cannot exec() %r: file not readable." % main_filename
    )

    python_program = find_python_binary(module_space)
    if python_program is None:
        raise AssertionError("Could not find python binary: " + _PYTHON_BINARY)

    # The python interpreter should always be under runfiles, but double check.
    # We don't want to accidentally create symlinks elsewhere.
    if not python_program.startswith(module_space):
        raise AssertionError(
            "Program's venv binary not under runfiles: {python_program}"
        )

    if os.path.isabs(_PYTHON_BINARY_ACTUAL):
        symlink_to = _PYTHON_BINARY_ACTUAL
    elif "/" in _PYTHON_BINARY_ACTUAL:
        symlink_to = os.path.join(module_space, _PYTHON_BINARY_ACTUAL)
    else:
        symlink_to = search_path(_PYTHON_BINARY_ACTUAL)
        if not symlink_to:
            raise AssertionError(
                f"Python interpreter to use not found on PATH: {_PYTHON_BINARY_ACTUAL}"
            )

    # The bin/ directory may not exist if it is empty.
    os.makedirs(os.path.dirname(python_program), exist_ok=True)
    try:
        os.symlink(symlink_to, python_program)
    except OSError as e:
        raise Exception(
            f"Unable to create venv python interpreter symlink: {python_program} -> {symlink_to}"
        ) from e

    # Some older Python versions on macOS (namely Python 3.7) may unintentionally
    # leave this environment variable set after starting the interpreter, which
    # causes problems with Python subprocesses correctly locating sys.executable,
    # which subsequently causes failure to launch on Python 3.11 and later.
    if "__PYVENV_LAUNCHER__" in os.environ:
        del os.environ["__PYVENV_LAUNCHER__"]

    new_env.update((key, val) for key, val in os.environ.items() if key not in new_env)

    workspace = None
    # If RUN_UNDER_RUNFILES equals 1, it means we need to
    # change directory to the right runfiles directory.
    # (So that the data files are accessible)
    if os.environ.get("RUN_UNDER_RUNFILES") == "1":
        workspace = os.path.join(module_space, _WORKSPACE_NAME)

    sys.stdout.flush()
    execute_file(
        python_program,
        main_filename,
        args,
        new_env,
        module_space,
        workspace,
    )


if __name__ == "__main__":
    main()
