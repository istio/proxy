#!/usr/bin/env bash

set -euo pipefail

TARGET="${CURRENT_TOOLCHAIN_FILES_TEST_INPUT}"
OPTION="${CURRENT_TOOLCHAIN_FILES_TEST_KIND}"

# To parse this argument on windows it must be wrapped in quotes but
# these quotes should not be passed to grep. Remove them here.
PATTERN="$(echo -n "${CURRENT_TOOLCHAIN_FILES_TEST_PATTERN}" | sed "s/'//g")"

if [[ "${OPTION}" == "executable" ]]; then
    # Clippy requires this environment variable is set
    export SYSROOT=""

    "${TARGET}" --version
    "${TARGET}" --version | grep "${PATTERN}"
    exit 0
fi

if [[ "${OPTION}" == "files" ]]; then
    cat "${TARGET}"
    grep "${PATTERN}" "${TARGET}"
    exit 0
fi

>&2 echo "Unexpected option: ${OPTION}"
exit 1
