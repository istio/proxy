#!/bin/bash
#
# TODO: use bazel integration test framework and rewrite this to be run by `bazel test`
set -euo pipefail

TMPDIR="$(mktemp -d)"
trap 'rm -rf -- "$TMPDIR"' EXIT

_log() {
    echo "INFO: $*"
}

main() {
    # First change the directory to workspace root
    cd "$(dirname "$0")"/..

    local -r version="${1:-0.0.0}"

    # Then build the release artifact
    local -r tarball=$(
        bazel run \
            --stamp --embed_label "$version" \
            //:release -- release
    )

    _log "Extracting the tarball into a temporary directory to run examples"
    tar -xvf "$tarball" -C "$TMPDIR"

    pushd $TMPDIR

    # Then run examples with the packaged artifacts
    examples=(
        check_glob
        optional_attributes
    )

    for example in "${examples[@]}"; do
        _log "Running an example with the generated archive"
        pushd "examples/$example"
        bazel \
            test \
            --override_module rules_shellcheck="$TMPDIR" \
            ...
        popd
    done

    popd
    _log "Success"
}

main "$@"
