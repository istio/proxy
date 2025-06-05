#!/usr/bin/env bash

set -e -o pipefail

TARGET=${1}
shift
FILTER=${1}

if [[ -z "$FILTER" ]]; then
    FILTER=.
else
    shift
fi

$JQ_BIN "${FILTER}" "${@}" < "$TARGET"
