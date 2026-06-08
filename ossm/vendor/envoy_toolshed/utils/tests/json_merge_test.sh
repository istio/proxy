#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JQ="${JQ_BIN:-jq}"

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

compare_json "${SCRIPT_DIR}/test_merge.json" "${SCRIPT_DIR}/data/expected_merge.json" "basic merge"
compare_json "${SCRIPT_DIR}/test_yaml_merge.json" "${SCRIPT_DIR}/data/expected_yaml_merge.json" "yaml merge"
compare_json "${SCRIPT_DIR}/test_filter.json" "${SCRIPT_DIR}/data/expected_filter.json" "custom filter"

echo "All tests passed!"
