#!/bin/sh

# Don't set -e because we don't have robust trapping and printing of errors.
set -u

# We use /bin/sh rather than /bin/bash for portability. See discussion here:
# https://groups.google.com/forum/?nomobile=true#!topic/bazel-dev/4Ql_7eDcLC0
# We do lose the ability to set -o pipefail.

FAILURE_HEADER="\
Error occurred while attempting to use the deprecated Python toolchain \
(@rules_python//python/runtime_env_toolchain:all)."

die() {
  echo "$FAILURE_HEADER" 1>&2
  echo "$1" 1>&2
  exit 1
}

# We use `command -v` to locate the Python interpreter command on PATH.
#
# A tricky situation happens when this wrapper is invoked as part of running a
# tool, e.g. passing a py_binary target to `ctx.actions.run()`. Bazel will unset
# the PATH variable. Then the shell will see there's no PATH and initialize its
# own, sometimes without exporting it. This causes `command -v` to fail and this
# script to think there's no Python interpreter installed. To avoid this we
# explicitly pass PATH to each `command -v` invocation. We can't just export PATH
# because that would modify the environment seen by the final user Python
# program.
#
# See also:
#
#     https://github.com/bazelbuild/continuous-integration/issues/578
#     https://github.com/bazelbuild/bazel/issues/8414
#     https://github.com/bazelbuild/bazel/issues/8415

# Try the "python3" command name first, then fall back on "python".
PYTHON_BIN="$(PATH="$PATH" command -v python3 2> /dev/null)"
if [ -z "${PYTHON_BIN:-}" ]; then
  PYTHON_BIN="$(PATH="$PATH" command -v python 2>/dev/null)"
fi
if [ -z "${PYTHON_BIN:-}" ]; then
  die "Neither 'python3' nor 'python' were found on the target \
platform's PATH, which is:

$PATH

Please ensure an interpreter is available on this platform (and marked \
executable), or else register an appropriate Python toolchain as per the \
documentation for py_runtime_pair \
(https://github.com/bazel-contrib/rules_python/blob/master/docs/python.md#py_runtime_pair)."
fi

# Because this is a wrapper script that invokes Python, it prevents Python from
# detecting virtualenvs like normal (i.e. using the venv symlink to find the
# real interpreter). To work around this, we have to manually detect the venv,
# then trick the interpreter into understanding we're in a virtual env.
self_dir=$(dirname "$0")
if [ -e "$self_dir/pyvenv.cfg" ] || [ -e "$self_dir/../pyvenv.cfg" ]; then
  case "$0" in
    /*)
      venv_bin="$0"
      ;;
    *)
      venv_bin="$PWD/$0"
      ;;
  esac

  if [ ! -e "$PYTHON_BIN" ]; then
    die "ERROR: Python interpreter does not exist: $PYTHON_BIN"
  fi
  # PYTHONEXECUTABLE is also used because switching argv0 doesn't fully trick
  # the pyenv wrappers.
  # NOTE: The PYTHONEXECUTABLE envvar only works for non-Mac starting in Python 3.11
  export PYTHONEXECUTABLE="$venv_bin"
  # Python looks at argv[0] to determine sys.executable, so set that to the venv
  # binary, not the actual one invoked.
  # NOTE: exec -a would be simpler, but isn't posix-compatible, and dash shell
  # (Ubuntu/debian default) doesn't support it; see #3009.
  exec sh -c "$PYTHON_BIN \$@" "$venv_bin" "$@"
else
  exec "$PYTHON_BIN" "$@"
fi
