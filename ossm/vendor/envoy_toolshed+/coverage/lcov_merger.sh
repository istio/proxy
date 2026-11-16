#!/usr/bin/env bash
# Per-test lcov merger (`--coverage_output_generator`) that strips the records the
# combined report does not read.
#
# Bazel runs the merger once per test (in the coverage postprocessing spawn) to turn the
# raw coverage data into the test's `coverage.dat`. Every one of those files describes
# the whole instrumented binary, and in a large C++ tree most of the bytes are function
# (`FN`/`FNDA`) and branch (`BRDA`) records: thousands of mangled names per file, repeated
# for every test. Line coverage, which is what the thresholds are validated against, is a
# small fraction. Stripping the rest shrinks every per-test file several-fold before it is
# uploaded, downloaded and merged into the combined report.
#
# Zero-hit `DA` lines are kept: the baseline report only lists files, so a line no test
# hits would otherwise disappear from the combined report rather than count as uncovered.
#
# When the merger runs inside the test action itself (no split postprocessing), the
# coverage directory it reads is one of the test's output directories, and everything in it
# is uploaded with the test's results: the raw profile, the unfiltered `llvm-cov export`
# and the indexed profile, several hundred MB per test that nothing reads again once the
# report is merged. The merger is the last step of that pipeline, so it removes them.
# In a split postprocessing spawn (IS_COVERAGE_SPAWN=1) the directory is an input of a
# separate action and is left alone.
#
# Arguments are passed through unchanged to Bazel's merger. The merged report is filtered
# in place afterwards, so the wrapper is transparent to `collect_coverage.sh`.

set -euo pipefail

# --- begin runfiles.bash initialization v3 ---
f=bazel_tools/tools/bash/runfiles/runfiles.bash
# shellcheck disable=SC1090
source "${RUNFILES_DIR:-/dev/null}/$f" 2>/dev/null ||
  source "$(grep -sm1 "^$f " "${RUNFILES_MANIFEST_FILE:-/dev/null}" | cut -f2- -d' ')" 2>/dev/null ||
  source "$0.runfiles/$f" 2>/dev/null ||
  source "$(grep -sm1 "^$f " "$0.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null ||
  { echo >&2 "ERROR: cannot find runfiles.bash"; exit 1; }
# --- end runfiles.bash initialization v3 ---

fail() { echo "ERROR: $*" >&2; exit 1; }

# Substituted at analysis time.
MERGER_RLOCATION="{{MERGER}}"
STRIP_RECORDS="{{STRIP}}"
CLEAN_COVERAGE_DIR="{{CLEAN}}"

output_file=
coverage_dir=
for arg in "$@"; do
    case "$arg" in
        --output_file=*) output_file="${arg#*=}" ;;
        --coverage_dir=*) coverage_dir="${arg#*=}" ;;
    esac
done
[[ -n "$output_file" ]] || fail "--output_file is required"
[[ -n "$STRIP_RECORDS" ]] || fail "No records configured to strip"

merger="$(rlocation "$MERGER_RLOCATION")" || merger=
[[ -n "$merger" && -e "$merger" ]] || fail "Merger not found in runfiles: $MERGER_RLOCATION"

# Bazel's merger is a java_binary inside this binary's runfiles tree. Its launcher looks for
# its own tree next to itself unless told where to look.
runfiles_root="${RUNFILES_DIR:-}"
if [[ -z "$runfiles_root" && -d "$0.runfiles" ]]; then
    runfiles_root="$0.runfiles"
fi
if [[ -n "$runfiles_root" ]]; then
    export RUNFILES_DIR="$runfiles_root"
    export JAVA_RUNFILES="$runfiles_root"
fi

"$merger" "$@"

physical_dir() { (cd -- "$1" 2>/dev/null && pwd -P); }

if [[ "$CLEAN_COVERAGE_DIR" == 1 && "${IS_COVERAGE_SPAWN:-}" != 1 && -n "$coverage_dir" && -d "$coverage_dir" ]]; then
    had_output=0
    [[ -f "$output_file" ]] && had_output=1
    coverage_dir="$(physical_dir "$coverage_dir")"
    # Should Bazel have placed the merged report inside the directory, at any depth, keep the
    # top-level entry that holds it.
    keep=
    output_parent="$(physical_dir "$(dirname -- "$output_file")")" || output_parent=
    if [[ -n "$output_parent" ]]; then
        output_path="${output_parent}/$(basename -- "$output_file")"
        if [[ "$output_path" == "$coverage_dir"/* ]]; then
            relative="${output_path#"$coverage_dir"/}"
            keep="${coverage_dir}/${relative%%/*}"
        fi
    fi
    for entry in "$coverage_dir"/* "$coverage_dir"/.[!.]* "$coverage_dir"/..?*; do
        [[ -e "$entry" || -L "$entry" ]] || continue
        [[ "$entry" == "$keep" ]] && continue
        rm -rf -- "$entry"
    done
    if [[ "$had_output" == 1 && ! -f "$output_file" ]]; then
        fail "Cleaning ${coverage_dir} removed the merged report ${output_file}"
    fi
fi

[[ -f "$output_file" ]] || exit 0

# `^(FN|FNDA|...):` from the configured list.
pattern="^(${STRIP_RECORDS// /|}):"
filtered="${output_file}.strip.$$"
trap 'rm -f -- "$filtered"' EXIT
# Byte-wise, locale-independent: a stray non-UTF-8 byte must not turn the report into
# grep's "binary file matches" message. grep exits 1 when nothing survives; an empty
# report is still a valid report.
LC_ALL=C grep -a -v -E "$pattern" "$output_file" > "$filtered" || [[ $? -eq 1 ]]
mv -f "$filtered" "$output_file"
