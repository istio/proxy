#!/bin/bash
# Test for bazel-update.sh: verifies that both hyphenated and non-hyphenated
# dependency names are handled correctly.

set -euo pipefail

# ---------------------------------------------------------------------------
# Locate runfiles when running under Bazel
# ---------------------------------------------------------------------------
if [ -n "${TEST_SRCDIR:-}" ]; then
    if [ -d "${TEST_SRCDIR}/envoy_toolshed" ]; then
        RUNFILES_DIR="${TEST_SRCDIR}/envoy_toolshed"
    else
        RUNFILES_DIR="${TEST_SRCDIR}/_main"
    fi
    UPDATE_SCRIPT="${RUNFILES_DIR}/dependency/bazel-update.sh"
    DEP_DATA="${RUNFILES_DIR}/dependency/test/testdata/dependency_versions.json"
    VERSIONS_TEMPLATE="${RUNFILES_DIR}/dependency/test/testdata/versions.bzl"
else
    # Running directly from the repository
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    UPDATE_SCRIPT="$(cd "${SCRIPT_DIR}/.." && pwd)/bazel-update.sh"
    DEP_DATA="${SCRIPT_DIR}/testdata/dependency_versions.json"
    VERSIONS_TEMPLATE="${SCRIPT_DIR}/testdata/versions.bzl"
fi

# ---------------------------------------------------------------------------
# Set up a fake curl that returns predictable content so sha256 is known.
# The sha256 of the string "fake-archive-content\n" is computed below.
# ---------------------------------------------------------------------------
FAKE_CURL_CONTENT="fake-archive-content"
EXPECTED_SHA="$(printf '%s\n' "${FAKE_CURL_CONTENT}" | sha256sum | cut -d' ' -f1)"

FAKE_CURL_DIR="$(mktemp -d)"
trap 'rm -rf "${FAKE_CURL_DIR}"' EXIT

cat > "${FAKE_CURL_DIR}/curl" <<'EOF'
#!/bin/bash
# Fake curl: ignore all arguments and print predictable content.
printf 'fake-archive-content\n'
EOF
chmod +x "${FAKE_CURL_DIR}/curl"

# Prepend the fake curl directory to PATH so bazel-update.sh picks it up.
export PATH="${FAKE_CURL_DIR}:${PATH}"

# ---------------------------------------------------------------------------
# Helper: run a single test case
# ---------------------------------------------------------------------------
run_test() {
    local test_name="$1"
    local dep_name="$2"
    local new_version="$3"

    echo ""
    echo "Running test: ${test_name}"

    # Work in a fresh temp directory each time.
    local work_dir
    work_dir="$(mktemp -d)"

    # Copy the versions file to the work dir so the updater can modify it.
    local versions_file="${work_dir}/versions.bzl"
    cp "${VERSIONS_TEMPLATE}" "${versions_file}"

    # Set variables the script expects.
    export BUILD_WORKSPACE_DIRECTORY="${work_dir}"

    # Run the updater.
    if ! bash "${UPDATE_SCRIPT}" \
            "versions.bzl" \
            "${DEP_DATA}" \
            "${dep_name}" \
            "${new_version}"; then
        echo "  ✗ FAIL: ${test_name} — updater script exited with error"
        rm -rf "${work_dir}"
        return 1
    fi

    # Verify the version was updated.
    if grep -q "\"version\": \"${new_version}\"" "${versions_file}"; then
        echo "  ✓ version updated to ${new_version}"
    else
        echo "  ✗ FAIL: ${test_name} — version not updated to ${new_version}"
        echo "  File contents:"
        cat "${versions_file}"
        rm -rf "${work_dir}"
        return 1
    fi

    # Verify the sha256 was updated to the expected value.
    if grep -q "\"sha256\": \"${EXPECTED_SHA}\"" "${versions_file}"; then
        echo "  ✓ sha256 updated to ${EXPECTED_SHA}"
    else
        echo "  ✗ FAIL: ${test_name} — sha256 not updated to expected value"
        echo "  Expected sha: ${EXPECTED_SHA}"
        echo "  File contents:"
        cat "${versions_file}"
        rm -rf "${work_dir}"
        return 1
    fi

    rm -rf "${work_dir}"
    echo "  PASS: ${test_name}"
    return 0
}

# ---------------------------------------------------------------------------
# Test cases
# ---------------------------------------------------------------------------
failures=0

run_test "non-hyphenated dep (bazel_skylib)" "bazel_skylib" "1.5.0" \
    || ((failures++))

run_test "hyphenated dep (rules-shell)" "rules-shell" "0.7.0" \
    || ((failures++))

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo ""
echo "================================"
if [ "${failures}" -eq 0 ]; then
    echo "All tests passed ✓"
    exit 0
else
    echo "${failures} test(s) FAILED ✗"
    exit 1
fi
