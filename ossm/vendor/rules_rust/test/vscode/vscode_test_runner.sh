#!/usr/bin/env bash

# This script creates a temporary workspace and generates `launch.json`
# files for each test subdirectory, then runs integration tests.

set -euo pipefail

if [[ -z "${BUILD_WORKSPACE_DIRECTORY:-}" ]]; then
    >&2 echo "This script should be run under Bazel"
    exit 1
fi

PACKAGE_NAME="$1"
if [[ -z "${PACKAGE_NAME:-}" ]]; then
    >&2 echo "The first argument should be the package name of the test target"
    exit 1
fi

function generate_workspace() {
    local workspace_root="$1"
    local package_dir="$2"
    local temp_dir="$(mktemp -d -t rules_rust_test_vscode-XXXXXXXXXX)"
    local new_workspace="${temp_dir}/rules_rust_test_vscode"

    mkdir -p "${new_workspace}"
    cat <<EOF >"${new_workspace}/MODULE.bazel"
module(
    name = "rules_rust_test_vscode",
    version = "0.0.0",
)
bazel_dep(name = "rules_rust", version = "0.0.0")
local_path_override(
    module_name = "rules_rust",
    path = "${BUILD_WORKSPACE_DIRECTORY}",
)

bazel_dep(
    name = "bazel_skylib",
    version = "1.8.2",
)

rust = use_extension("@rules_rust//rust:extensions.bzl", "rust")
use_repo(rust, "rust_toolchains")
register_toolchains("@rust_toolchains//:all")

vscode_test = use_extension("//test/vscode/3rdparty:extensions.bzl", "vscode_test", dev_dependency = True)
use_repo(
    vscode_test,
EOF

    grep -hr "rtvsc" "${BUILD_WORKSPACE_DIRECTORY}/MODULE.bazel" >> "${new_workspace}/MODULE.bazel"
    echo ")" >> "${new_workspace}/MODULE.bazel"

    cat <<EOF >"${new_workspace}/.bazelrc"
build --keep_going
test --test_output=errors
EOF

    if [[ -f "${workspace_root}/.bazelversion" ]]; then
        cp "${workspace_root}/.bazelversion" "${new_workspace}/.bazelversion"
    fi

    # Copy test directories to the root of temp workspace
    for test_dir in "${workspace_root}/${package_dir}"/*_test; do
        if [[ -d "${test_dir}" ]]; then
            local test_name="$(basename "${test_dir}")"
            mkdir -p "${new_workspace}/${test_name}"
            cp -r "${test_dir}"/* "${new_workspace}/${test_name}/"
        fi
    done

    # Copy integration_tests and 3rdparty to test/vscode/
    mkdir -p "${new_workspace}/${package_dir}"
    cp -r "${workspace_root}/${package_dir}/integration_tests" "${new_workspace}/${package_dir}/"
    cp -r "${workspace_root}/${package_dir}/3rdparty" "${new_workspace}/${package_dir}/"

    echo "${new_workspace}"
}

function run_vscode_tests() {
    local workspace="$1"
    local rust_log="info"
    if [[ -n "${VSCODE_TEST_DEBUG:-}" ]]; then
        rust_log="debug"
    fi

    pushd "${workspace}" &>/dev/null

    # Test 1: only_binaries_test
    echo "Testing only_binaries_test..."
    set +e
    RUST_LOG="${rust_log}" bazel run @rules_rust//tools/vscode:gen_launch_json -- --output only_binaries_test/.vscode/launch.json //only_binaries_test/...
    local exit_code=$?
    set -e
    if [[ ${exit_code} -ne 0 ]]; then
        echo "ERROR: Failed to generate launch.json for only_binaries_test"
        popd &>/dev/null
        return 1
    fi
    echo "Running integration test for only_binaries_test..."
    LAUNCH_JSON="$(pwd)/only_binaries_test/.vscode/launch.json" bazel test //test/vscode/integration_tests:only_binaries_test --test_env=LAUNCH_JSON

    # Test 2: only_tests_test
    echo "Testing only_tests_test..."
    set +e
    RUST_LOG="${rust_log}" bazel run @rules_rust//tools/vscode:gen_launch_json -- --output only_tests_test/.vscode/launch.json //only_tests_test/...
    local exit_code=$?
    set -e
    if [[ ${exit_code} -ne 0 ]]; then
        echo "ERROR: Failed to generate launch.json for only_tests_test"
        popd &>/dev/null
        return 1
    fi
    echo "Running integration test for only_tests_test..."
    LAUNCH_JSON="$(pwd)/only_tests_test/.vscode/launch.json" bazel test //test/vscode/integration_tests:only_tests_test --test_env=LAUNCH_JSON

    # Test 3: no_targets_test (should fail)
    echo "Testing no_targets_test (expecting error)..."
    set +e
    RUST_LOG="${rust_log}" bazel run @rules_rust//tools/vscode:gen_launch_json -- //no_targets_test/...
    local exit_code=$?
    set -e
    if [[ ${exit_code} -eq 0 ]]; then
        echo "ERROR: Expected no_targets_test to fail but it succeeded"
        popd &>/dev/null
        return 1
    fi
    echo "no_targets_test failed as expected"

    # Test 4: binaries_and_tests_test
    echo "Testing binaries_and_tests_test..."
    set +e
    RUST_LOG="${rust_log}" bazel run @rules_rust//tools/vscode:gen_launch_json -- --output binaries_and_tests_test/.vscode/launch.json //binaries_and_tests_test/...
    local exit_code=$?
    set -e
    if [[ ${exit_code} -ne 0 ]]; then
        echo "ERROR: Failed to generate launch.json for binaries_and_tests_test"
        popd &>/dev/null
        return 1
    fi
    echo "Running integration test for binaries_and_tests_test..."
    LAUNCH_JSON="$(pwd)/binaries_and_tests_test/.vscode/launch.json" bazel test //test/vscode/integration_tests:binaries_and_tests_test --test_env=LAUNCH_JSON

    popd &>/dev/null
    echo "All tests passed!"
}

function cleanup() {
    local workspace="$1"
    pushd "${workspace}" &>/dev/null
    bazel clean --expunge --async
    popd &>/dev/null
    if [[ -z "${VSCODE_TEST_DEBUG:-}" ]]; then
        rm -rf "${workspace}"
    else
        echo "Debug workspace preserved at: ${workspace}"
    fi
}

function run_test_suite() {
    local temp_workspace="$(generate_workspace "${BUILD_WORKSPACE_DIRECTORY}" "${PACKAGE_NAME}")"
    echo "Generated workspace: ${temp_workspace}"

    run_vscode_tests "${temp_workspace}"

    echo "Done"
    cleanup "${temp_workspace}"
}

run_test_suite
