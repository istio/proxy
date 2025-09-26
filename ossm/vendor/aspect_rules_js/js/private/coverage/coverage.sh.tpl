#!/usr/bin/env bash

set -o pipefail -o errexit -o nounset

function logf_stderr {
    local format_string="$1\n"
    shift
    # shellcheck disable=SC2059
    printf "$format_string" "$@" >&2
}

function logf_fatal {
    printf "FATAL: " >&2
    logf_stderr "$@"
}

# ==============================================================================
# Initialize RUNFILES environment variable
# ==============================================================================
{{initialize_runfiles}}

# When --experimental_split_coverage_postprocessing is enabled, bazel creates
# separate runfiles directory for the coverage merger. 
# When --experimental_split_coverage_postprocessing is disabled we observe the issue 
# in https://github.com/bazelbuild/bazel/issues/4033
if [ $SPLIT_COVERAGE_POST_PROCESSING == 1 ]; then
    RUNFILES=$(_normalize_path "$LCOV_MERGER.runfiles")
fi

export RUNFILES

# ==============================================================================
# Prepare to run coverage program
# ==============================================================================

entry_point="$RUNFILES/{{workspace_name}}/{{entry_point_path}}"
if [ ! -f "$entry_point" ]; then
    printf "FATAL: the entry_point '%s' not found in runfiles" "$entry_point"
    exit 1
fi

node="$RUNFILES/{{workspace_name}}/{{node}}"
if [ ! -f "$node" ]; then
    logf_fatal "node binary '%s' not found in runfiles" "$node"
    exit 1
fi
if [ ! -x "$node" ]; then
    logf_fatal "node binary '%s' is not executable" "$node"
    exit 1
fi

# ==============================================================================
# Run the coverage program
# ==============================================================================

"$node" "$entry_point"
