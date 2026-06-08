#!/bin/bash
# Checks that a cross-compiled binary is of the expected ELF architecture.
# The BINARY and EXPECTED_ARCH environment variables must be set.
set -euo pipefail

: "${BINARY:?BINARY must be set to the path of the compiled binary}"
: "${EXPECTED_ARCH:?EXPECTED_ARCH must be set to the expected architecture (e.g. aarch64, x86-64)}"

FILE_OUTPUT="$(file -L "${BINARY}")"
echo "file output: ${FILE_OUTPUT}"

if echo "${FILE_OUTPUT}" | grep -q "${EXPECTED_ARCH}"; then
    echo "PASS: binary is ${EXPECTED_ARCH}"
else
    echo "FAIL: expected ${EXPECTED_ARCH} in file output, got: ${FILE_OUTPUT}"
    exit 1
fi
