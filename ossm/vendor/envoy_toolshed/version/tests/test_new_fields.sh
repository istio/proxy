#!/bin/bash
# Test that module_versions outputs the required new fields for Envoy compatibility
# Tests for: version, minimum_version, registry fields

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

OUTPUT_FILE="${SCRIPT_DIR}/test_versions.json"

if [ ! -f "$OUTPUT_FILE" ]; then
    echo "ERROR: Output file $OUTPUT_FILE does not exist" >&2
    exit 1
fi

failures=0

echo "Testing module_versions output structure..."
echo ""

# Test 1: Check that 'version' field exists (not 'resolved_version')
echo "Test 1: Checking 'version' field exists for all modules"
has_version=$("$JQ" -r 'all(.[]; has("version"))' "$OUTPUT_FILE")
if [ "$has_version" = "true" ]; then
    echo "  ✓ PASS: All modules have 'version' field"
else
    echo "  ✗ FAIL: Some modules are missing 'version' field"
    ((failures++))
fi

# Test 2: Check that 'resolved_version' field does NOT exist (renamed to 'version')
echo "Test 2: Checking resolved_version field does not exist"
has_resolved=$("$JQ" -r 'any(.[]; has("resolved_version"))' "$OUTPUT_FILE")
if [ "$has_resolved" = "false" ]; then
    echo "  ✓ PASS: No modules have 'resolved_version' field (correctly renamed to 'version')"
else
    echo "  ✗ FAIL: Some modules still have 'resolved_version' field"
    ((failures++))
fi

# Test 3: Check that 'minimum_version' field exists
echo "Test 3: Checking 'minimum_version' field exists for all modules"
has_minimum=$("$JQ" -r 'all(.[]; has("minimum_version"))' "$OUTPUT_FILE")
if [ "$has_minimum" = "true" ]; then
    echo "  ✓ PASS: All modules have 'minimum_version' field"
else
    echo "  ✗ FAIL: Some modules are missing 'minimum_version' field"
    ((failures++))
fi

# Test 4: Check that 'registry' field exists
echo "Test 4: Checking 'registry' field exists for all modules"
has_registry=$("$JQ" -r 'all(.[]; has("registry"))' "$OUTPUT_FILE")
if [ "$has_registry" = "true" ]; then
    echo "  ✓ PASS: All modules have 'registry' field"
else
    echo "  ✗ FAIL: Some modules are missing 'registry' field"
    ((failures++))
fi

# Test 5: Verify specific example (aspect_bazel_lib)
echo "Test 5: Checking specific module (aspect_bazel_lib) has correct structure"
if "$JQ" -e '.aspect_bazel_lib' "$OUTPUT_FILE" > /dev/null 2>&1; then
    aspect_version=$("$JQ" -r '.aspect_bazel_lib.version' "$OUTPUT_FILE")
    aspect_min=$("$JQ" -r '.aspect_bazel_lib.minimum_version' "$OUTPUT_FILE")
    aspect_registry=$("$JQ" -r '.aspect_bazel_lib.registry' "$OUTPUT_FILE")

    if [ -n "$aspect_version" ] && [ -n "$aspect_min" ] && [ -n "$aspect_registry" ]; then
        echo "  ✓ PASS: aspect_bazel_lib has all required fields"
        echo "    - version: $aspect_version"
        echo "    - minimum_version: $aspect_min"
        echo "    - registry: $aspect_registry"
    else
        echo "  ✗ FAIL: aspect_bazel_lib missing some fields"
        ((failures++))
    fi
else
    echo "  ✗ FAIL: aspect_bazel_lib module not found in output"
    ((failures++))
fi

# Test 6: Check that version can differ from minimum_version (rules_python case)
echo "Test 6: Checking support for different minimum_version vs version"
if "$JQ" -e '.rules_python' "$OUTPUT_FILE" > /dev/null 2>&1; then
    python_version=$("$JQ" -r '.rules_python.version' "$OUTPUT_FILE")
    python_min=$("$JQ" -r '.rules_python.minimum_version' "$OUTPUT_FILE")

    echo "  rules_python: minimum_version=$python_min, version=$python_version"
    if [ -n "$python_version" ] && [ -n "$python_min" ]; then
        echo "  ✓ PASS: rules_python has both minimum_version and version (may differ)"
    else
        echo "  ✗ FAIL: rules_python missing version or minimum_version"
        ((failures++))
    fi
else
    # Not a failure - just informational
    echo "  ⚠ INFO: rules_python not in test data (test skipped)"
fi

echo ""
if [ "$failures" -eq 0 ]; then
    echo "All field validation tests passed!"
    exit 0
else
    echo "$failures test(s) failed"
    exit 1
fi
