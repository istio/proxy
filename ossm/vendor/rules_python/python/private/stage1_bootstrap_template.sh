#!/bin/bash

set -e

if [[ -n "${RULES_PYTHON_BOOTSTRAP_VERBOSE:-}" ]]; then
  set -x
fi

# runfiles-relative path
STAGE2_BOOTSTRAP="%stage2_bootstrap%"

# runfiles-relative path to python interpreter to use
PYTHON_BINARY='%python_binary%'
# The path that PYTHON_BINARY should symlink to.
# runfiles-relative path, absolute path, or single word.
# Only applicable for zip files or when venv is recreated at runtime.
PYTHON_BINARY_ACTUAL="%python_binary_actual%"

# 0 or 1
IS_ZIPFILE="%is_zipfile%"
# 0 or 1
RECREATE_VENV_AT_RUNTIME="%recreate_venv_at_runtime%"

if [[ "$IS_ZIPFILE" == "1" ]]; then
  # NOTE: Macs have an old version of mktemp, so we must use only the
  # minimal functionality of it.
  zip_dir=$(mktemp -d)

  if [[ -n "$zip_dir" && -z "${RULES_PYTHON_BOOTSTRAP_VERBOSE:-}" ]]; then
    trap 'rm -fr "$zip_dir"' EXIT
  fi
  # unzip emits a warning and exits with code 1 when there is extraneous data,
  # like this bootstrap prelude code, but otherwise successfully extracts, so
  # we have to ignore its exit code and suppress stderr.
  # The alternative requires having to copy ourselves elsewhere with the prelude
  # stripped (because zip can't extract from a stream). We avoid that because
  # it's wasteful.
  ( unzip -q -d "$zip_dir" "$0" 2>/dev/null || true )

  RUNFILES_DIR="$zip_dir/runfiles"
  if [[ ! -d "$RUNFILES_DIR" ]]; then
    echo "Runfiles dir not found: zip extraction likely failed"
    echo "Run with RULES_PYTHON_BOOTSTRAP_VERBOSE=1 to aid debugging"
    exit 1
  fi

else
  function find_runfiles_root() {
    if [[ -n "${RUNFILES_DIR:-}" ]]; then
      echo "$RUNFILES_DIR"
      return 0
    elif [[ "${RUNFILES_MANIFEST_FILE:-}" = *".runfiles_manifest" ]]; then
      echo "${RUNFILES_MANIFEST_FILE%%.runfiles_manifest}.runfiles"
      return 0
    elif [[ "${RUNFILES_MANIFEST_FILE:-}" = *".runfiles/MANIFEST" ]]; then
      echo "${RUNFILES_MANIFEST_FILE%%.runfiles/MANIFEST}.runfiles"
      return 0
    fi

    stub_filename="$1"
    # A relative path to our executable, as happens with
    # a build action or bazel-bin/ invocation
    if [[ "$stub_filename" != /* ]]; then
      stub_filename="$PWD/$stub_filename"
    fi
    while true; do
      module_space="${stub_filename}.runfiles"
      if [[ -d "$module_space" ]]; then
        echo "$module_space"
        return 0
      fi
      if [[ "$stub_filename" == *.runfiles/* ]]; then
        echo "${stub_filename%.runfiles*}.runfiles"
        return 0
      fi
      if [[ ! -L "$stub_filename" ]]; then
        break
      fi
      target=$(realpath $maybe_runfiles_root)
      stub_filename="$target"
    done
    echo >&2 "Unable to find runfiles directory for $1"
    exit 1
  }
  RUNFILES_DIR=$(find_runfiles_root $0)
fi


function find_python_interpreter() {
  runfiles_root="$1"
  interpreter_path="$2"
  if [[ "$interpreter_path" == /* ]]; then
    # An absolute path, i.e. platform runtime
    echo "$interpreter_path"
  elif [[ "$interpreter_path" == */* ]]; then
    # A runfiles-relative path
    echo "$runfiles_root/$interpreter_path"
  else
    # A plain word, e.g. "python3". Rely on searching PATH
    echo "$interpreter_path"
  fi
}

python_exe=$(find_python_interpreter $RUNFILES_DIR $PYTHON_BINARY)

# Zip files have to re-create the venv bin/python3 symlink because they
# don't contain it already.
if [[ "$IS_ZIPFILE" == "1" ]]; then
  use_exec=0
  # It should always be under runfiles, but double check this. We don't
  # want to accidentally create symlinks elsewhere.
  if [[ "$python_exe" != $RUNFILES_DIR/* ]]; then
    echo >&2 "ERROR: Program's venv binary not under runfiles: $python_exe"
    exit 1
  fi
  if [[ "$PYTHON_BINARY_ACTUAL" == /* ]]; then
    # An absolute path, i.e. platform runtime, e.g. /usr/bin/python3
    symlink_to=$PYTHON_BINARY_ACTUAL
  elif [[ "$PYTHON_BINARY_ACTUAL" == */* ]]; then
    # A runfiles-relative path
    symlink_to=$RUNFILES_DIR/$PYTHON_BINARY_ACTUAL
  else
    # A plain word, e.g. "python3". Symlink to where PATH leads
    symlink_to=$(which $PYTHON_BINARY_ACTUAL)
    # Guard against trying to symlink to an empty value
    if [[ $? -ne 0 ]]; then
      echo >&2 "ERROR: Python to use not found on PATH: $PYTHON_BINARY_ACTUAL"
      exit 1
    fi
  fi
  # The bin/ directory may not exist if it is empty.
  mkdir -p "$(dirname $python_exe)"
  ln -s "$symlink_to" "$python_exe"
elif [[ "$RECREATE_VENV_AT_RUNTIME" == "1" ]]; then
  if [[ -n "$RULES_PYTHON_EXTRACT_ROOT" ]]; then
    use_exec=1
    # Use our runfiles path as a unique, reusable, location for the
    # binary-specific venv being created.
    venv="$RULES_PYTHON_EXTRACT_ROOT/$(dirname $(dirname $PYTHON_BINARY))"
    mkdir -p $RULES_PYTHON_EXTRACT_ROOT
  else
    # Re-exec'ing can't be used because we have to clean up the temporary
    # venv directory that is created.
    use_exec=0
    venv=$(mktemp -d)
    if [[ -n "$venv" && -z "${RULES_PYTHON_BOOTSTRAP_VERBOSE:-}" ]]; then
      trap 'rm -fr "$venv"' EXIT
    fi
  fi

  if [[ "$PYTHON_BINARY_ACTUAL" == /* ]]; then
    # An absolute path, i.e. platform runtime, e.g. /usr/bin/python3
    symlink_to=$PYTHON_BINARY_ACTUAL
  elif [[ "$PYTHON_BINARY_ACTUAL" == */* ]]; then
    # A runfiles-relative path
    symlink_to="$RUNFILES_DIR/$PYTHON_BINARY_ACTUAL"
  else
    # A plain word, e.g. "python3". Symlink to where PATH leads
    symlink_to=$(which $PYTHON_BINARY_ACTUAL)
    # Guard against trying to symlink to an empty value
    if [[ $? -ne 0 ]]; then
      echo >&2 "ERROR: Python to use not found on PATH: $PYTHON_BINARY_ACTUAL"
      exit 1
    fi
  fi
  mkdir -p "$venv/bin"
  # Match the basename; some tools, e.g. pyvenv key off the executable name
  python_exe="$venv/bin/$(basename $PYTHON_BINARY_ACTUAL)"
  if [[ ! -e "$python_exe" ]]; then
    ln -s "$symlink_to" "$python_exe"
  fi
  runfiles_venv="$RUNFILES_DIR/$(dirname $(dirname $PYTHON_BINARY))"
  if [[ ! -e "$venv/pyvenv.cfg" ]]; then
    ln -s "$runfiles_venv/pyvenv.cfg" "$venv/pyvenv.cfg"
  fi
  if [[ ! -e "$venv/lib" ]]; then
    ln -s "$runfiles_venv/lib" "$venv/lib"
  fi
else
  use_exec=1
fi

# At this point, we should have a valid reference to the interpreter.
# Check that so we can give an nicer failure if things went wrong.
if [[ ! -x "$python_exe" ]]; then
  if [[ ! -e "$python_exe" ]]; then
    echo >&2 "ERROR: Python interpreter not found: $python_exe"
    ls -l $python_exe >&2
    exit 1
  elif [[ ! -x "$python_exe" ]]; then
    echo >&2 "ERROR: Python interpreter not executable: $python_exe"
    exit 1
  fi
fi

stage2_bootstrap="$RUNFILES_DIR/$STAGE2_BOOTSTRAP"

declare -a interpreter_env
declare -a interpreter_args

# Don't prepend a potentially unsafe path to sys.path
# See: https://docs.python.org/3.11/using/cmdline.html#envvar-PYTHONSAFEPATH
# NOTE: Only works for 3.11+
# We inherit the value from the outer environment in case the user wants to
# opt-out of using PYTHONSAFEPATH. To opt-out, they have to set
# `PYTHONSAFEPATH=` (empty string). This is because Python treats the empty
# value as false, and any non-empty value as true.
# ${FOO+WORD} expands to empty if $FOO is undefined, and WORD otherwise.
if [[ -z "${PYTHONSAFEPATH+x}" ]]; then
  # ${FOO-WORD} expands to WORD if $FOO is undefined, and $FOO otherwise
  interpreter_env+=("PYTHONSAFEPATH=${PYTHONSAFEPATH-1}")
fi

if [[ "$IS_ZIPFILE" == "1" ]]; then
  interpreter_args+=("-XRULES_PYTHON_ZIP_DIR=$zip_dir")
fi

export RUNFILES_DIR

command=(
  env
  "${interpreter_env[@]}"
  "$python_exe"
  "${interpreter_args[@]}"
  "$stage2_bootstrap"
  "$@"
)

# We use `exec` instead of a child process so that signals sent directly (e.g.
# using `kill`) to this process (the PID seen by the calling process) are
# received by the Python process. Otherwise, this process receives the signal
# and would have to manually propagate it.
# See https://github.com/bazelbuild/rules_python/issues/2043#issuecomment-2215469971
# for more information.
#
# However, we can't use exec when there is cleanup to do afterwards. Control
# must return to this process so it can run the trap handlers. Such cases
# occur when zip mode or recreate_venv_at_runtime creates temporary files.
if [[ "$use_exec" == "0" ]]; then
  "${command[@]}"
  exit $?
else
  exec "${command[@]}"
fi
