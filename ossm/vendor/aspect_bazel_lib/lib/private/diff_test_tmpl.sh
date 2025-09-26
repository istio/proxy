#!/usr/bin/env bash
set -o errexit -o nounset -o pipefail
escape() {
  echo "$1" |
    sed 's/&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g; s/"/\&quot;/g; s/'"'"'/\&#39;/g' |
    awk 1 ORS='&#10;' # preserve newlines
}
fail() {
  cat <<EOF >"${XML_OUTPUT_FILE:-/dev/null}"
<?xml version="1.0" encoding="UTF-8"?>
<testsuites name="$(escape "{name}")" tests="1" failures="1">
  <testsuite name="$(escape "{name}")" tests="1" failures="1" id="0">
    <testcase name="$(escape "{name}")" assertions="1" status="failed">
      <failure message="$(escape "$1")" type="diff"></failure>
    </testcase>
  </testsuite>
</testsuites>
EOF
  echo >&2 "FAIL: $1"
  exit 1
}
resolve_exec_root() {
  local RUNFILES_PARENT
  RUNFILES_PARENT=$(dirname "$RUNFILES_DIR")
  local BIN_DIR
  BIN_DIR="${RUNFILES_PARENT%$BUILD_FILE_DIR}"
  local EXEC_ROOT
  EXEC_ROOT=$(dirname $(dirname $(dirname "${BIN_DIR}")))

  echo -n "$EXEC_ROOT"
}
find_file() {
  local F_RAW="$1"
  local F="$2"
  local RF=

  if [[ -f "$TEST_SRCDIR/$F1" || -d "$TEST_SRCDIR/$F" ]]; then
    RF="$TEST_SRCDIR/$F"
  elif [[ -d "${RUNFILES_DIR:-/dev/null}" && "${RUNFILES_MANIFEST_ONLY:-}" != 1 ]]; then
    EXEC_ROOT=$(resolve_exec_root)
    if [[ -e "$EXEC_ROOT/$F_RAW" ]]; then
      RF="$EXEC_ROOT/$F_RAW"
    else
      RF="$RUNFILES_DIR/$F1"
    fi
  elif [[ -f "${RUNFILES_MANIFEST_FILE:-/dev/null}" ]]; then
    RF="$(grep -F -m1 "$F " "$RUNFILES_MANIFEST_FILE" | sed 's/^[^ ]* //')"
  else
    echo >&2 "ERROR: could not find \"${F_RAW}\""
    exit 1
  fi

  echo -n "$RF"
}
BUILD_FILE_DIR="$(dirname "{build_file_path}")"
F1="{file1}"
F2="{file2}"
[[ "$F1" =~ ^external/ ]] && F1="${F1#external/}" || F1="$TEST_WORKSPACE/$F1"
[[ "$F2" =~ ^external/ ]] && F2="${F2#external/}" || F2="$TEST_WORKSPACE/$F2"
RF1="$(find_file {file1} "$F1")"
RF2="$(find_file {file2} "$F2")"
DF1=
DF2=
[[ ! -d "$RF1" ]] || DF1=1
[[ ! -d "$RF2" ]] || DF2=1
if [[ "$DF1" ]] && [[ ! "$DF2" ]]; then
  echo >&2 "ERROR: cannot compare a directory \"{file1}\" against a file \"{file2}\""
  exit 1
fi
if [[ ! "$DF1" ]] && [[ "$DF2" ]]; then
  echo >&2 "ERROR: cannot compare a file \"{file1}\" against a directory \"{file2}\""
  exit 1
fi
if [[ "$DF1" ]] || [[ "$DF2" ]]; then
  if ! diff {diff_args} -r "$RF1" "$RF2"; then
    fail "directories \"{file1}\" and \"{file2}\" differ. {fail_msg}"
  fi
else
  if ! diff {diff_args} "$RF1" "$RF2"; then
    fail "files \"{file1}\" and \"{file2}\" differ. {fail_msg}"
  fi
fi
