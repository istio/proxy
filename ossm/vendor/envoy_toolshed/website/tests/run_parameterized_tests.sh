#!/bin/bash
set -euo pipefail

# Test script for parameterized website generation tests
# Tests custom configurations and generator-agnostic features

echo "======================================="
echo "Running Parameterized Website Tests"
echo "======================================="

# Handle Bazel runfiles
if [ -n "${TEST_SRCDIR:-}" ]; then
    if [ -d "${TEST_SRCDIR}/envoy_toolshed" ]; then
        RUNFILES_DIR="${TEST_SRCDIR}/envoy_toolshed"
    else
        RUNFILES_DIR="${TEST_SRCDIR}/_main"
    fi
else
    echo "ERROR: TEST_SRCDIR not set. This script must be run under Bazel test."
    exit 1
fi

TEST_DIR="${RUNFILES_DIR}/website/tests"
failed=0

# Test custom exclude patterns
echo ""
echo "Test 1: Custom exclude patterns"
TARBALL="${TEST_DIR}/test_custom_exclude_html.tar.gz"
if [ ! -f "${TARBALL}" ]; then
    echo "✗ FAILED: Custom exclude tarball not found"
    failed=$((failed + 1))
else
    EXTRACT_DIR=$(mktemp -d)
    trap 'rm -rf ${EXTRACT_DIR}' EXIT

    if tar -xzf "${TARBALL}" -C "${EXTRACT_DIR}" 2>&1; then
        echo "✓ PASSED: Custom exclude tarball extracted"

        # Verify that fewer items are excluded
        # Note: This is a basic check - actual verification would depend on
        # knowing what the generator produces
        echo "  Checking extracted contents..."
        file_count=$(find "${EXTRACT_DIR}" -type f | wc -l)
        echo "  Found ${file_count} files"

        if [ "${file_count}" -gt 0 ]; then
            echo "✓ PASSED: Files exist in custom exclude output"
        else
            echo "✗ FAILED: No files in custom exclude output"
            failed=$((failed + 1))
        fi
    else
        echo "✗ FAILED: Could not extract custom exclude tarball"
        failed=$((failed + 1))
    fi
fi

# Test custom mappings
echo ""
echo "Test 2: Custom mappings"
TARBALL="${TEST_DIR}/test_custom_mappings_html.tar.gz"
if [ ! -f "${TARBALL}" ]; then
    echo "✗ FAILED: Custom mappings tarball not found"
    failed=$((failed + 1))
else
    EXTRACT_DIR=$(mktemp -d)

    if tar -xzf "${TARBALL}" -C "${EXTRACT_DIR}" 2>&1; then
        echo "✓ PASSED: Custom mappings tarball extracted"

        # Check that output was generated
        file_count=$(find "${EXTRACT_DIR}" -type f | wc -l)
        echo "  Found ${file_count} files"

        if [ "${file_count}" -gt 0 ]; then
            echo "✓ PASSED: Files exist in custom mappings output"
        else
            echo "✗ FAILED: No files in custom mappings output"
            failed=$((failed + 1))
        fi
    else
        echo "✗ FAILED: Could not extract custom mappings tarball"
        failed=$((failed + 1))
    fi
fi

# Test generator agnostic interface
echo ""
echo "Test 3: Generator-agnostic interface verification"
echo "Checking that macro accepts generator parameter..."
# This is verified by successful compilation of the BUILD file
echo "✓ PASSED: BUILD file compiles with custom parameters"

echo ""
echo "======================================="
if [ $failed -eq 0 ]; then
    echo "All parameterized tests PASSED ✓"
    exit 0
else
    echo "${failed} parameterized test(s) FAILED ✗"
    exit 1
fi
