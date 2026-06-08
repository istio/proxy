#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Ensure JQ is available
if [ -z "${JQ_BIN:-}" ]; then
    if ! command -v jq &> /dev/null; then
        echo "ERROR: jq is not available. JQ_BIN environment variable is not set and 'jq' is not in PATH." >&2
        exit 1
    fi
    JQ="jq"
else
    JQ="${JQ_BIN}"
fi

compare_json() {
    local actual="$1"
    local expected="$2"
    local name="$3"

    # Normalize and compare JSON
    actual_normalized=$("$JQ" -S . "$actual")
    expected_normalized=$("$JQ" -S . "$expected")

    if [ "$actual_normalized" = "$expected_normalized" ]; then
        echo "PASS: $name"
    else
        echo "FAIL: $name"
        echo "Expected: $expected_normalized"
        echo "Actual: $actual_normalized"
        exit 1
    fi
}

compare_json "${SCRIPT_DIR}/test_versions.json" "${SCRIPT_DIR}/data/expected_versions.json" "module versions extraction"

echo "All tests passed!"
