#!/usr/bin/env bash

set -euo pipefail

# Skip the first argument which is expected to be `--`
shift

args=()

for arg in "$@"; do
    # Check if the argument contains "${PWD}" and replace it with the actual value of PWD
    if [[ "${arg}" == *'${pwd}'* ]]; then
        arg="${arg//\$\{pwd\}/$PWD}"
    fi
    args+=("${arg}")
done

exec "${args[@]}"
