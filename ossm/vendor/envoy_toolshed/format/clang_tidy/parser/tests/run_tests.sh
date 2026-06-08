#!/bin/bash
set -euo pipefail

# Handle Bazel runfiles
if [ -n "${TEST_SRCDIR:-}" ]; then
    # Running under Bazel test
    # Support both WORKSPACE (envoy_toolshed) and bzlmod (_main) repository names
    if [ -d "${TEST_SRCDIR}/envoy_toolshed" ]; then
        RUNFILES_DIR="${TEST_SRCDIR}/envoy_toolshed"
    else
        RUNFILES_DIR="${TEST_SRCDIR}/_main"
    fi
    SCRIPT_DIR="${RUNFILES_DIR}/format/clang_tidy/parser/tests"
    PARSER="${RUNFILES_DIR}/format/clang_tidy/parser/parse_clang_tidy.jq"
else
    # Running directly
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PARSER="${SCRIPT_DIR}/../parse_clang_tidy.jq"
fi

JQ="${JQ_BIN:-jq}"

if ! command -v "$JQ" &> /dev/null; then
    echo "jq binary not found: ${JQ}" >&2
    exit 1
fi

run_test() {
    local input_file="$1"
    local expected_file="$2"
    local test_name="$3"
    echo ""
    echo "Running test: ${test_name}"
    local actual_output
    actual_output=$(mktemp)
    if ! ${JQ} -Rf "${PARSER}" < "${input_file}" > "${actual_output}" 2>&1; then
        echo "✗ Test FAILED: Parser error"
        cat "${actual_output}"
        rm -f "${actual_output}"
        return 1
    fi
    if diff -q "${expected_file}" "${actual_output}" > /dev/null 2>&1; then
        echo "✓ Test PASSED: ${test_name}"
        rm -f "${actual_output}"
        return 0
    else
        echo "✗ Test FAILED: ${test_name}"
        echo ""
        echo "Diff:"
        diff -u "${expected_file}" "${actual_output}" || true
        echo ""
        echo "Actual output:"
        cat "${actual_output}"
        rm -f "${actual_output}"
        return 1
    fi
}

failed=0
if ! run_test "${SCRIPT_DIR}/sample_input.txt" "${SCRIPT_DIR}/expected_output.json" "Sample input (multiple diagnostics)"; then
    failed=$((failed + 1))
fi
if ! run_test "${SCRIPT_DIR}/simple_error.txt" "${SCRIPT_DIR}/simple_error_expected.json" "Simple error"; then
    failed=$((failed + 1))
fi
if ! run_test "${SCRIPT_DIR}/no_diagnostics.txt" "${SCRIPT_DIR}/no_diagnostics_expected.json" "No diagnostics"; then
    failed=$((failed + 1))
fi

echo ""
echo "================================"
if [ $failed -eq 0 ]; then
    echo "All tests PASSED ✓"
    exit 0
else
    echo "${failed} test(s) FAILED ✗"
    exit 1
fi
