#!/usr/bin/env bash

set -e

if [[ -n "${RULES_PYTHON_BOOTSTRAP_VERBOSE:-}" ]]; then
  set -x
fi

# runfiles-root-relative path
BUNDLED_PYEXE_PATH="%BUNDLED_PYEXE_PATH%"
# Absolute path or single word
EXTERNAL_PYEXE_PATH="%EXTERNAL_PYEXE_PATH%"
# runfiles-root-relative path
STAGE2_BOOTSTRAP="%STAGE2_BOOTSTRAP%"
EXTRACT_DIR="%EXTRACT_DIR%"
ZIP_HASH="%ZIP_HASH%"
declare -a INTERPRETER_ARGS_FROM_TARGET=(
%INTERPRETER_ARGS%
)

declare -a interpreter_env
declare -a interpreter_args
declare -a additional_interpreter_args

if [[ -z "${PYTHONSAFEPATH+x}" ]]; then
  # ${FOO-WORD} expands to WORD if $FOO is undefined, and $FOO otherwise
  interpreter_env+=("PYTHONSAFEPATH=${PYTHONSAFEPATH-1}")
fi


if [[ -n "${RULES_PYTHON_ADDITIONAL_INTERPRETER_ARGS}" ]]; then
  read -a additional_interpreter_args <<< "${RULES_PYTHON_ADDITIONAL_INTERPRETER_ARGS}"
  interpreter_args+=("${additional_interpreter_args[@]}")
  unset RULES_PYTHON_ADDITIONAL_INTERPRETER_ARGS
fi


if [[ -n "$RULES_PYTHON_EXTRACT_ROOT" ]]; then
  zip_dir="$RULES_PYTHON_EXTRACT_ROOT/$EXTRACT_DIR/$ZIP_HASH"
  if [[ ! -e "$zip_dir/__main__.py" ]]; then
    mkdir -p "$zip_dir"
    # Unzip emits a warning and exits 1 with the prelude
    ( unzip -q -d "$zip_dir" "$0" 2>/dev/null || true )
  fi
else
  # NOTE: Macs have an old version of mktemp, so we must use only the
  # minimal functionality of it.
  zip_dir=$(mktemp -d)
  # Unzip emits a warning and exits 1 with the prelude
  ( unzip -q -d "$zip_dir" "$0" 2>/dev/null || true )
  if [[ -n "$zip_dir" && -z "${RULES_PYTHON_BOOTSTRAP_VERBOSE:-}" ]]; then
    trap 'rm -fr "$zip_dir"' EXIT
  fi
fi

export RUNFILES_DIR="$zip_dir/runfiles"
if [[ ! -d "$RUNFILES_DIR" ]]; then
  echo "Runfiles dir not found: zip extraction likely failed" 1>&2
  echo "Run with RULES_PYTHON_BOOTSTRAP_VERBOSE=1 to aid debugging" 1>&2
  exit 1
fi

if [[ -n "$BUNDLED_PYEXE_PATH" ]]; then
  python_exe="$RUNFILES_DIR/$BUNDLED_PYEXE_PATH"
else
  python_exe="$EXTERNAL_PYEXE_PATH"
fi

command=(
  env
  "${interpreter_env[@]}"
  "$python_exe"
  "-XRULES_PYTHON_ZIP_DIR=$zip_dir"
  "${interpreter_args[@]}"
  "${INTERPRETER_ARGS_FROM_TARGET[@]}"
  "$RUNFILES_DIR/$STAGE2_BOOTSTRAP"
  "$@"
)

# NOTE: because exec isn't used, signals don't propagate to the child
# TODO: Use exec and let the program handle cleanup. Without exec,
# signals don't propagate to the child nicely.
# See https://github.com/bazel-contrib/rules_python/issues/2043#issuecomment-2215469971
# for more information.
"${command[@]}"
# Explicit exit is needed because the implicit next line the zip file this
# template is prepended to.
exit 0
