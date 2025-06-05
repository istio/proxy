# This is a "stage 2" bootstrap. We can assume we've running under the desired
# interpreter, with some of the basic interpreter options/envvars set.
# However, more setup is required to make the app's real main file runnable.

import sys

# By default the Python interpreter prepends the directory containing this
# script (following symlinks) to the import path. This is the cause of #9239,
# and is a special case of #7091.
#
# Python 3.11 introduced an PYTHONSAFEPATH (-P) option that disables this
# behaviour, which we set in the stage 1 bootstrap.
# So the prepended entry needs to be removed only if the above option is either
# unset or not supported by the interpreter.
# NOTE: This can be removed when Python 3.10 and below is no longer supported
if not getattr(sys.flags, "safe_path", False):
    del sys.path[0]

import contextlib
import os
import re
import runpy
import uuid

# ===== Template substitutions start =====
# We just put them in one place so its easy to tell which are used.

# Runfiles-relative path to the main Python source file.
MAIN = "%main%"

# ===== Template substitutions end =====


# Return True if running on Windows
def is_windows():
    return os.name == "nt"


def get_windows_path_with_unc_prefix(path):
    path = path.strip()

    # No need to add prefix for non-Windows platforms.
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
    if path.startswith(unicode_prefix):
        return path

    # os.path.abspath returns a normalized absolute path
    return unicode_prefix + os.path.abspath(path)


def search_path(name):
    """Finds a file in a given search path."""
    search_path = os.getenv("PATH", os.defpath).split(os.pathsep)
    for directory in search_path:
        if directory:
            path = os.path.join(directory, name)
            if os.path.isfile(path) and os.access(path, os.X_OK):
                return path
    return None


def is_verbose():
    return bool(os.environ.get("RULES_PYTHON_BOOTSTRAP_VERBOSE"))


def print_verbose(*args, mapping=None, values=None):
    if is_verbose():
        if mapping is not None:
            for key, value in sorted((mapping or {}).items()):
                print(
                    "bootstrap: stage 2:",
                    *args,
                    f"{key}={value!r}",
                    file=sys.stderr,
                    flush=True,
                )
        elif values is not None:
            for i, v in enumerate(values):
                print(
                    "bootstrap: stage 2:",
                    *args,
                    f"[{i}] {v!r}",
                    file=sys.stderr,
                    flush=True,
                )
        else:
            print("bootstrap: stage 2:", *args, file=sys.stderr, flush=True)


def print_verbose_coverage(*args):
    """Print output if VERBOSE_COVERAGE is non-empty in the environment."""
    if is_verbose_coverage():
        print("bootstrap: stage 2: coverage:", *args, file=sys.stderr, flush=True)


def is_verbose_coverage():
    """Returns True if VERBOSE_COVERAGE is non-empty in the environment."""
    return os.environ.get("VERBOSE_COVERAGE") or is_verbose()


def find_runfiles_root(main_rel_path):
    """Finds the runfiles tree."""
    # When the calling process used the runfiles manifest to resolve the
    # location of this stub script, the path may be expanded. This means
    # argv[0] may no longer point to a location inside the runfiles
    # directory. We should therefore respect RUNFILES_DIR and
    # RUNFILES_MANIFEST_FILE set by the caller.
    runfiles_dir = os.environ.get("RUNFILES_DIR", None)
    if not runfiles_dir:
        runfiles_manifest_file = os.environ.get("RUNFILES_MANIFEST_FILE", "")
        if runfiles_manifest_file.endswith(
            ".runfiles_manifest"
        ) or runfiles_manifest_file.endswith(".runfiles/MANIFEST"):
            runfiles_dir = runfiles_manifest_file[:-9]
    # Be defensive: the runfiles dir should contain our main entry point. If
    # it doesn't, then it must not be our runfiles directory.
    if runfiles_dir and os.path.exists(os.path.join(runfiles_dir, main_rel_path)):
        return runfiles_dir

    stub_filename = sys.argv[0]
    if not os.path.isabs(stub_filename):
        stub_filename = os.path.join(os.getcwd(), stub_filename)

    while True:
        module_space = stub_filename + (".exe" if is_windows() else "") + ".runfiles"
        if os.path.isdir(module_space):
            return module_space

        runfiles_pattern = r"(.*\.runfiles)" + (r"\\" if is_windows() else "/") + ".*"
        matchobj = re.match(runfiles_pattern, stub_filename)
        if matchobj:
            return matchobj.group(1)

        if not os.path.islink(stub_filename):
            break
        target = os.readlink(stub_filename)
        if os.path.isabs(target):
            stub_filename = target
        else:
            stub_filename = os.path.join(os.path.dirname(stub_filename), target)

    raise AssertionError("Cannot find .runfiles directory for %s" % sys.argv[0])


def runfiles_envvar(module_space):
    """Finds the runfiles manifest or the runfiles directory.

    Returns:
      A tuple of (var_name, var_value) where var_name is either 'RUNFILES_DIR' or
      'RUNFILES_MANIFEST_FILE' and var_value is the path to that directory or
      file, or (None, None) if runfiles couldn't be found.
    """
    # If this binary is the data-dependency of another one, the other sets
    # RUNFILES_MANIFEST_FILE or RUNFILES_DIR for our sake.
    runfiles = os.environ.get("RUNFILES_MANIFEST_FILE", None)
    if runfiles:
        return ("RUNFILES_MANIFEST_FILE", runfiles)

    runfiles = os.environ.get("RUNFILES_DIR", None)
    if runfiles:
        return ("RUNFILES_DIR", runfiles)

    # Look for the runfiles "output" manifest, argv[0] + ".runfiles_manifest"
    runfiles = module_space + "_manifest"
    if os.path.exists(runfiles):
        return ("RUNFILES_MANIFEST_FILE", runfiles)

    # Look for the runfiles "input" manifest, argv[0] + ".runfiles/MANIFEST"
    # Normally .runfiles_manifest and MANIFEST are both present, but the
    # former will be missing for zip-based builds or if someone copies the
    # runfiles tree elsewhere.
    runfiles = os.path.join(module_space, "MANIFEST")
    if os.path.exists(runfiles):
        return ("RUNFILES_MANIFEST_FILE", runfiles)

    # If running in a sandbox and no environment variables are set, then
    # Look for the runfiles  next to the binary.
    if module_space.endswith(".runfiles") and os.path.isdir(module_space):
        return ("RUNFILES_DIR", module_space)

    return (None, None)


def instrumented_file_paths():
    """Yields tuples of realpath of each instrumented file with the relative path."""
    manifest_filename = os.environ.get("COVERAGE_MANIFEST")
    if not manifest_filename:
        return
    with open(manifest_filename, "r") as manifest:
        for line in manifest:
            filename = line.strip()
            if not filename:
                continue
            try:
                realpath = os.path.realpath(filename)
            except OSError:
                print(
                    "Could not find instrumented file {}".format(filename),
                    file=sys.stderr,
                    flush=True,
                )
                continue
            if realpath != filename:
                print_verbose_coverage("Fixing up {} -> {}".format(realpath, filename))
                yield (realpath, filename)


def unresolve_symlinks(output_filename):
    # type: (str) -> None
    """Replace realpath of instrumented files with the relative path in the lcov output.

    Though we are asking coveragepy to use relative file names, currently
    ignore that for purposes of generating the lcov report (and other reports
    which are not the XML report), so we need to go and fix up the report.

    This function is a workaround for that issue. Once that issue is fixed
    upstream and the updated version is widely in use, this should be removed.

    See https://github.com/nedbat/coveragepy/issues/963.
    """
    substitutions = list(instrumented_file_paths())
    if substitutions:
        unfixed_file = output_filename + ".tmp"
        os.rename(output_filename, unfixed_file)
        with open(unfixed_file, "r") as unfixed:
            with open(output_filename, "w") as output_file:
                for line in unfixed:
                    if line.startswith("SF:"):
                        for realpath, filename in substitutions:
                            line = line.replace(realpath, filename)
                    output_file.write(line)
        os.unlink(unfixed_file)


def _run_py(main_filename, *, args, cwd=None):
    # type: (str, str, list[str], dict[str, str]) -> ...
    """Executes the given Python file using the various environment settings."""

    orig_argv = sys.argv
    orig_cwd = os.getcwd()
    try:
        sys.argv = [main_filename] + args
        if cwd:
            os.chdir(cwd)
        print_verbose("run_py: cwd:", os.getcwd())
        print_verbose("run_py: sys.argv: ", values=sys.argv)
        print_verbose("run_py: os.environ:", mapping=os.environ)
        print_verbose("run_py: sys.path:", values=sys.path)
        runpy.run_path(main_filename, run_name="__main__")
    finally:
        os.chdir(orig_cwd)
        sys.argv = orig_argv


@contextlib.contextmanager
def _maybe_collect_coverage(enable):
    print_verbose_coverage("enabled:", enable)
    if not enable:
        yield
        return

    import uuid

    import coverage

    coverage_dir = os.environ["COVERAGE_DIR"]
    unique_id = uuid.uuid4()

    # We need for coveragepy to use relative paths.  This can only be configured
    # using an rc file.
    rcfile_name = os.path.join(coverage_dir, ".coveragerc_{}".format(unique_id))
    print_verbose_coverage("coveragerc file:", rcfile_name)
    with open(rcfile_name, "w") as rcfile:
        rcfile.write(
            """[run]
relative_files = True
"""
        )
    try:
        cov = coverage.Coverage(
            config_file=rcfile_name,
            branch=True,
            # NOTE: The messages arg controls what coverage prints to stdout/stderr,
            # which can interfere with the Bazel coverage command. Enabling message
            # output is only useful for debugging coverage support.
            messages=is_verbose_coverage(),
            omit=[
                # Pipes can't be read back later, which can cause coverage to
                # throw an error when trying to get its source code.
                "/dev/fd/*",
                # The mechanism for finding third-party packages in coverage-py
                # only works for installed packages, not for runfiles. e.g:
                #'$HOME/.local/lib/python3.10/site-packages',
                # '/usr/lib/python',
                # '/usr/lib/python3.10/site-packages',
                # '/usr/local/lib/python3.10/dist-packages'
                # see https://github.com/nedbat/coveragepy/blob/bfb0c708fdd8182b2a9f0fc403596693ef65e475/coverage/inorout.py#L153-L164
                "*/external/*",
            ],
        )
        cov.start()
        try:
            yield
        finally:
            cov.stop()
            lcov_path = os.path.join(coverage_dir, "pylcov.dat")
            print_verbose_coverage("generating lcov from:", lcov_path)
            cov.lcov_report(
                outfile=lcov_path,
                # Ignore errors because sometimes instrumented files aren't
                # readable afterwards. e.g. if they come from /dev/fd or if
                # they were transient code-under-test in /tmp
                ignore_errors=True,
            )
            if os.path.isfile(lcov_path):
                unresolve_symlinks(lcov_path)
    finally:
        try:
            os.unlink(rcfile_name)
        except OSError as err:
            # It's possible that the profiled program might execute another Python
            # binary through a wrapper that would then delete the rcfile.  Not much
            # we can do about that, besides ignore the failure here.
            print_verbose_coverage("Error removing temporary coverage rc file:", err)


def main():
    print_verbose("initial argv:", values=sys.argv)
    print_verbose("initial cwd:", os.getcwd())
    print_verbose("initial environ:", mapping=os.environ)
    print_verbose("initial sys.path:", values=sys.path)

    main_rel_path = MAIN
    if is_windows():
        main_rel_path = main_rel_path.replace("/", os.sep)

    module_space = find_runfiles_root(main_rel_path)
    print_verbose("runfiles root:", module_space)

    # Recreate the "add main's dir to sys.path[0]" behavior to match the
    # system-python bootstrap / typical Python behavior.
    #
    # Without safe path enabled, when `python foo/bar.py` is run, python will
    # resolve the foo/bar.py symlink to its real path, then add the directory
    # of that path to sys.path. But, the resolved directory for the symlink
    # depends on if the file is generated or not.
    #
    # When foo/bar.py is a source file, then it's a symlink pointing
    # back to the client source directory. This means anything from that source
    # directory becomes importable, i.e. most code is importable.
    #
    # When foo/bar.py is a generated file, then it's a symlink pointing to
    # somewhere under bazel-out/.../bin, i.e. where generated files are. This
    # means only other generated files are importable (not source files).
    #
    # To replicate this behavior, we add main's directory within the runfiles
    # when safe path isn't enabled.
    if not getattr(sys.flags, "safe_path", False):
        prepend_path_entries = [
            os.path.join(module_space, os.path.dirname(main_rel_path))
        ]
    else:
        prepend_path_entries = []

    runfiles_envkey, runfiles_envvalue = runfiles_envvar(module_space)
    if runfiles_envkey:
        os.environ[runfiles_envkey] = runfiles_envvalue

    main_filename = os.path.join(module_space, main_rel_path)
    main_filename = get_windows_path_with_unc_prefix(main_filename)
    assert os.path.exists(main_filename), (
        "Cannot exec() %r: file not found." % main_filename
    )
    assert os.access(main_filename, os.R_OK), (
        "Cannot exec() %r: file not readable." % main_filename
    )

    sys.stdout.flush()

    sys.path[0:0] = prepend_path_entries

    if os.environ.get("COVERAGE_DIR"):
        import _bazel_site_init
        coverage_enabled = _bazel_site_init.COVERAGE_SETUP
    else:
        coverage_enabled = False

    with _maybe_collect_coverage(enable=coverage_enabled):
        # The first arg is this bootstrap, so drop that for the re-invocation.
        _run_py(main_filename, args=sys.argv[1:])
        sys.exit(0)


main()
