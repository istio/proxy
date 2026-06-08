#!/usr/bin/env bash

# This script creates temporary workspaces and generates `rust-project.json`
# files unique to the set of targets defined in the generated workspace.

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
    local temp_dir="$(mktemp -d -t rules_rust_test_rust_analyzer-XXXXXXXXXX)"
    local new_workspace="${temp_dir}/rules_rust_test_rust_analyzer"

    mkdir -p "${new_workspace}"
    cat <<EOF >"${new_workspace}/MODULE.bazel"
module(
    name = "rules_rust_test_rust_analyzer",
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

rust_analyzer_test = use_extension("//test/rust_analyzer/3rdparty:extensions.bzl", "rust_analyzer_test", dev_dependency = True)
use_repo(
    rust_analyzer_test,
EOF

    grep -hr "rtra" "${BUILD_WORKSPACE_DIRECTORY}/MODULE.bazel" >> "${new_workspace}/MODULE.bazel"
    echo ")" >> "${new_workspace}/MODULE.bazel"

    cat <<EOF >"${new_workspace}/.bazelrc"
build --keep_going
test --test_output=errors
# The 'strict' config is used to ensure extra checks are run on the test
# targets that would otherwise not run due to them being tagged as "manual".
# Note that that tag is stripped for this test.
build:strict --aspects=@rules_rust//rust:defs.bzl%rustfmt_aspect
build:strict --output_groups=+rustfmt_checks
build:strict --aspects=@rules_rust//rust:defs.bzl%rust_clippy_aspect
build:strict --output_groups=+clippy_checks
EOF

    if [[ -f "${workspace_root}/.bazelversion" ]]; then
        cp "${workspace_root}/.bazelversion" "${new_workspace}/.bazelversion"
    fi

    mkdir -p "${new_workspace}/${package_dir}"
    cp -r "${workspace_root}/${package_dir}/3rdparty" "${new_workspace}/${package_dir}/"

    echo "${new_workspace}"
}

function rust_analyzer_test() {
    local source_dir="$1"
    local workspace="$2"
    local generator_arg="$3"
    local rust_log="info"
    if [[ -n "${RUST_ANALYZER_TEST_DEBUG:-}" ]]; then
        rust_log="debug"
    fi

    echo "Testing '$(basename "${source_dir}")'"
    rm -f "${workspace}"/*.rs "${workspace}"/*.json "${workspace}"/*.bzl "${workspace}/BUILD.bazel" "${workspace}/BUILD.bazel-e"
    cp -r "${source_dir}"/* "${workspace}"

    # Drop the 'manual' tags
    if [ "$(uname)" == "Darwin" ]; then
        SEDOPTS=(-i '' -e)
    else
        SEDOPTS=(-i)
    fi
    sed ${SEDOPTS[@]} 's/"manual"//' "${workspace}/BUILD.bazel"

    pushd "${workspace}" &>/dev/null
    echo "Generating rust-project.json..."

    if [[ -n "${generator_arg}" ]]; then
        RUST_LOG="${rust_log}" bazel run "@rules_rust//tools/rust_analyzer:gen_rust_project" -- "${generator_arg}"
    else
        RUST_LOG="${rust_log}" bazel run "@rules_rust//tools/rust_analyzer:gen_rust_project"
    fi

    echo "Validating rust-project.json..."
    bazel run "@rules_rust//tools/rust_analyzer:validate" -- rust-project.json

    echo "Generating auto-discovery.json..."
    RUST_LOG="${rust_log}" bazel run "@rules_rust//tools/rust_analyzer:discover_bazel_rust_project" > auto-discovery.json

    echo "Building..."
    bazel build //...
    echo "Testing..."
    bazel test //...
    echo "Building with Aspects..."
    bazel build //... --config=strict
    popd &>/dev/null
}

function cleanup() {
    local workspace="$1"
    pushd "${workspace}" &>/dev/null
    bazel clean --expunge --async
    popd &>/dev/null
    if [[ -z "${RUST_ANALYZER_TEST_DEBUG:-}" ]]; then
        rm -rf "${workspace}"
    fi
}

function run_test_suite() {
    local temp_workspace="$(generate_workspace "${BUILD_WORKSPACE_DIRECTORY}" "${PACKAGE_NAME}")"
    echo "Generated workspace: ${temp_workspace}"

    for test_dir in "${BUILD_WORKSPACE_DIRECTORY}/${PACKAGE_NAME}"/*; do
        # Skip everything but directories
        if [[ ! -d "${test_dir}" ]]; then
            continue
        fi

        # Skip the 3rdparty directory
        if [[ "${test_dir}" == *"3rdparty"* ]]; then
            continue
        fi

        # Some tests have arguments that need to be passed to the rust-project.json generator.
        if [[ "${test_dir}" = "aspect_traversal_test" ]]; then
            test_arg="//mylib_test"
        elif [[ "${test_dir}" = "merging_crates_test" ]]; then
            test_arg="//mylib_test"
        else
            test_arg=""
        fi

        rust_analyzer_test "${test_dir}" "${temp_workspace}" "${test_arg}"
    done

    echo "Done"
    cleanup "${temp_workspace}"
}

run_test_suite
