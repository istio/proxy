#!/usr/bin/env bash
#
# For details, see:
# `@rules_rust//crate_universe/src/metadata/cargo_tree_resolver.rs - TreeResolver::create_rustc_wrapper`

set -euo pipefail

# When cargo is detecting the host configuration, the host target needs to be
# injected into the command.
if [[ "$@" == *"rustc - --crate-name ___ "* && "$@" != *" --target "* ]]; then
    exec "$@" --target "${HOST_TRIPLE}"
fi

# When querying info about the compiler, ensure the triple is mocked out to be
# the desired target triple for the host.
if [[ "$@" == *"rustc -Vv" || "$@" == *"rustc -vV" ]]; then
    set +e
    _RUSTC_OUTPUT="$($@)"
    _EXIT_CODE=$?
    set -e

    # Loop through each line of the output
    while IFS= read -r line; do
        # If the line starts with "host:", replace it with the new host value
        if [[ "${line}" == host:* ]]; then
            echo "host: ${HOST_TRIPLE}"
        else
            # Print the other lines unchanged
            echo "${line}"
        fi
    done <<<"${_RUSTC_OUTPUT}"

    exit ${_EXIT_CODE}
fi

# If there is nothing special to do then simply forward the call
exec "$@"
