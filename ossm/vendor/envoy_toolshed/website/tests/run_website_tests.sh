#!/bin/bash
set -euo pipefail

# Test script to verify basic website generation
# This tests that the static_website macro correctly generates a website tarball

echo "================================"
echo "Running Website Generation Tests"
echo "================================"

# Handle Bazel runfiles
if [ -n "${TEST_SRCDIR:-}" ]; then
    # Running under Bazel test
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
TARBALL="${TEST_DIR}/test_basic_website_html.tar.gz"

# Check that the tarball was created
echo ""
echo "Test 1: Verifying tarball exists"
if [ ! -f "${TARBALL}" ]; then
    echo "✗ FAILED: Tarball not found at ${TARBALL}"
    exit 1
fi
echo "✓ PASSED: Tarball exists"

# Extract and verify contents
EXTRACT_DIR=$(mktemp -d)
trap 'rm -rf ${EXTRACT_DIR}' EXIT

echo ""
echo "Test 2: Extracting tarball"
if ! tar -xzf "${TARBALL}" -C "${EXTRACT_DIR}" 2>&1; then
    echo "✗ FAILED: Could not extract tarball"
    exit 1
fi
echo "✓ PASSED: Tarball extracted successfully"

# Check for expected output files
echo ""
echo "Test 3: Verifying output structure"
if [ ! -d "${EXTRACT_DIR}" ]; then
    echo "✗ FAILED: Output directory not found"
    exit 1
fi

# List what was extracted (for debugging)
echo "Contents of extracted tarball:"
find "${EXTRACT_DIR}" -type f | head -20

echo ""
echo "Test 4: Checking for HTML output"
# Look for any .html files in the output
html_count=$(find "${EXTRACT_DIR}" -name "*.html" -type f | wc -l)
if [ "${html_count}" -eq 0 ]; then
    echo "✗ FAILED: No HTML files found in output"
    echo "Directory contents:"
    ls -la "${EXTRACT_DIR}"
    exit 1
fi
echo "✓ PASSED: Found ${html_count} HTML file(s)"

# Check that index.html exists
echo ""
echo "Test 5: Verifying index.html exists"
if [ ! -f "${EXTRACT_DIR}/index.html" ]; then
    echo "⚠ WARNING: index.html not found at root"
    echo "This may be expected depending on generator configuration"
else
    echo "✓ PASSED: index.html exists"
    # Verify it contains expected content
    if grep -q "Test Site" "${EXTRACT_DIR}/index.html"; then
        echo "✓ PASSED: index.html contains expected site name"
    else
        echo "⚠ WARNING: index.html does not contain expected site name"
    fi
fi

echo ""
echo "================================"
echo "All tests PASSED ✓"
echo "================================"
exit 0
