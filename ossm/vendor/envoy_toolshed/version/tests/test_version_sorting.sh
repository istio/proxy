#!/bin/bash
# Test semantic version sorting against known Bazel/BCR version orderings

set -euo pipefail

JQ="${JQ_BIN:-jq}"

# Test function
test_version_sort() {
    local test_name="$1"
    shift
    local expected_highest="${!#}"  # Last argument

    echo "Testing: $test_name"

    # Create JSON array
    local json_array
    json_array=$(printf '%s\n' "$@" | "$JQ" -R . | "$JQ" -s .)

    # Sort using our algorithm
    local sorted
    sorted=$( echo "$json_array" | "$JQ" 'def version_key: split(".") | map(split("-") | map(if test("^[0-9]+$") then tonumber else . end)); map({version: ., key: (. | version_key)}) | sort_by(.key) | map(.version)')

    # Check if last element is what we expect
    local result_last
    result_last=$(echo "$sorted" | "$JQ" -r '.[-1]')

    if [ "$result_last" = "$expected_highest" ]; then
        echo "  ✓ PASS: Highest version is $result_last"
        return 0
    else
        echo "  ✗ FAIL: Expected $expected_highest to be highest, got $result_last"
        echo "  Full sorted result: $sorted"
        return 1
    fi
}

failures=0

# Test 1: Basic semver
test_version_sort "Basic semver" "1.0.0" "1.2.0" "1.10.0" "2.0.0" || ((failures++))

# Test 2: Patch versions
test_version_sort "Patch versions" "1.0.0" "1.0.1" "1.0.2" "1.0.10" "1.0.11" || ((failures++))

# Test 3: Pre-release versions (these should be LESS than release)
# In proper semver and Bazel: 1.0.0-alpha < 1.0.0-beta < 1.0.0-rc1 < 1.0.0
echo "Testing: Pre-release versions (should be less than release)"
result=$(echo '["1.0.0", "1.0.0-rc1", "1.0.0-beta", "1.0.0-alpha"]' | "$JQ" 'def version_key: if contains("-") then (split("-") | {release: (.[0] | split(".") | map(if test("^[0-9]+$") then [0, tonumber] else [1, .] end)), has_prerelease: true, prerelease: (.[1:] | join("-") | split(".") | map(if test("^[0-9]+$") then [0, tonumber] else [1, .] end))}) else {release: (split(".") | map(if test("^[0-9]+$") then [0, tonumber] else [1, .] end)), has_prerelease: false, prerelease: []} end | [.release, (if .has_prerelease then 0 else 1 end), .prerelease]; map({version: ., key: (. | version_key)}) | sort_by(.key) | map(.version)')
echo "  Result: $result"
# The highest should be "1.0.0" (without prerelease)
highest=$(echo "$result" | "$JQ" -r '.[-1]')
if [ "$highest" = "1.0.0" ]; then
    echo "  ✓ PASS: Prerelease versions correctly sort before release"
    echo "  Expected order: 1.0.0-alpha < 1.0.0-beta < 1.0.0-rc1 < 1.0.0"
else
    echo "  ✗ FAIL: Expected 1.0.0 to be highest, got $highest"
    ((failures++))
fi

# Test 4: Date-based versions (used by some BCR modules like abseil-cpp)
test_version_sort "Date versions" "20210324.2" "20230802.0" "20230901.1" "20240116.1" || ((failures++))

# Test 5: Complex prerelease (from real BCR)
test_version_sort "Complex prerelease" "0.0.0" "0.0.0-20220923-a547704" "0.0.1" || ((failures++))

# Test 6: Real bazel_skylib versions
test_version_sort "Real bazel_skylib versions" \
    "1.0.3" "1.1.1" "1.2.0" "1.2.1" "1.3.0" "1.4.1" "1.4.2" \
    "1.5.0" "1.6.1" "1.7.0" "1.7.1" "1.8.1" "1.8.2" || ((failures++))

# Test 7: Real platforms versions
test_version_sort "Real platforms versions" \
    "0.0.4" "0.0.5" "0.0.6" "0.0.7" "0.0.8" "0.0.9" "0.0.10" "0.0.11" "1.0.0" || ((failures++))

echo ""
if [ "$failures" -eq 0 ]; then
    echo "All tests passed!"
    exit 0
else
    echo "$failures test(s) failed"
    exit 1
fi
