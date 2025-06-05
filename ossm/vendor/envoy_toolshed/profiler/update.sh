#!/usr/bin/env bash

set -e -o pipefail


BUILDOZER=${PWD}/$BUILDOZER
COMMANDS_PATH=${PWD}/$COMMANDS_PATH

cd "$REPO_PATH" || exit 1

$BUILDOZER  -f "${COMMANDS_PATH}"
