#!/usr/bin/env bash
# Usage: run_clang_tidy <OUTPUT> <CONFIG> [ARGS...]
set -ue

CLANG_TIDY_BIN=$1
shift

OUTPUT=$1
shift

CONFIG=$1
shift

# clang-tidy doesn't create a patchfile if there are no errors.
# make sure the output exists, and empty if there are no errors,
# so the build system will not be confused.
touch "$OUTPUT"
truncate -s 0 "$OUTPUT"

# if $CONFIG is provided by some external workspace, we need to
# place it in the current directory
test -e .clang-tidy || ln -s -f "$CONFIG" .clang-tidy

# echo "RUN: ${CLANG_TIDY_BIN} ${*}"

"${CLANG_TIDY_BIN}" "$@" |& grep -v "warnings generated" || :
