#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${BUILD_WORKSPACE_DIRECTORY:-}" ]]; then
    readonly out="${BUILD_WORKSPACE_DIRECTORY}/{{src_out}}"
else
    exit 1
fi
exec "{{args}}" --output-file "$out" "$@"
