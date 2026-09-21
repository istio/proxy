#!/bin/bash
# Copyright Red Hat, Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -eo pipefail

function init(){
  ROOT_DIR="$(pwd)"

  OUTPUT_BASE="/tmp/base"
  VENDOR_DIR="${ROOT_DIR}/ossm/vendor"
  BAZELRC="${ROOT_DIR}/ossm/bazelrc-vendor"

  rm -rf "${OUTPUT_BASE}" &&  mkdir -p "${OUTPUT_BASE}"
  rm -rf "${VENDOR_DIR}" &&  mkdir -p "${VENDOR_DIR}"
  : > "${BAZELRC}"

  # Remove symlinks to previous builds to avoid issues
  rm -f bazel-*


  IGNORE_LIST=(
        "antlr4-cpp-runtime"
        "bazel_tools"
        "cmake"
        "envoy_api"
        "envoy_build_config"
        "local_config"
        "local_jdk"
        "bazel_gazelle_go"
        "openssl"
        "llvm_toolchain"
        "llvm_minimal_linux"
        "go_sdk"
        "host_platform"
        "remotejdk"
        "rust"
        "nodejs"
        "rules_foreign_cc_framework_toolchain_freebsd_commands"
        "rules_foreign_cc_framework_toolchain_macos_commands"
        "rules_foreign_cc_framework_toolchain_windows_commands"
        "emscripten"
        "python3_12_host"
        "python3_12_x86_64"
        "python3_12_ppc"
        "python3_12_s390x"
        "python3_12_aarch64"
  )
}

function error() {
  echo "$@"
  exit 1
}

function validate() {
  if [ ! -f "WORKSPACE" ]; then
    error "Must run in the envoy/proxy dir"
  fi
}

function contains () {
  local e match="$1"
  shift
  for e; do [[ "$match" == "$e"* ]] && return 0; done
  return 1
}

function copy_files() {
  local cp_flags
  for f in "${OUTPUT_BASE}"/external/*; do
    if [ -d "${f}" ]; then
      repo_name=$(basename "${f}")
      if contains "${repo_name}" "${IGNORE_LIST[@]}" ; then
        continue
      fi

      cp_flags="-rL"
      if [ "${repo_name}" == "envoy_toolshed" ] || [[ "${repo_name}" == *"luajit2"* ]]; then
        cp_flags="-r"
      fi
      cp "${cp_flags}" "${f}" "${VENDOR_DIR}" || echo "Copy of ${f} failed. Ignoring..."
      echo "build --override_repository=${repo_name}=%workspace%/ossm/vendor/${repo_name}" >> "${BAZELRC}"
    fi
  done

  # Remove non-needed files and directories
  chmod -R +w "${VENDOR_DIR}"
  find "${VENDOR_DIR}" -name .git -type d -print0 | xargs -0 -r rm -rf
  find "${VENDOR_DIR}" -name .gitignore -type f -delete
  find "${VENDOR_DIR}" -name __pycache__ -type d -print0 | xargs -0 -r rm -rf
  find "${VENDOR_DIR}" -name '*.pyc' -delete
  find "${VENDOR_DIR}/v8" -name '*.bak' -delete
  rm -rf "${VENDOR_DIR}/boringssl/third_party" "${VENDOR_DIR}/boringssl/crypto/cipher/test"
}

function run_bazel() {
  # Fetch platform-specific tools for arm64 (not fetched automatically on x86_64
  # because these are execution-platform tools, not target-platform tools)
  bazel --output_base="${OUTPUT_BASE}" fetch @remote_java_tools_linux_aarch64//:all
  bazel --output_base="${OUTPUT_BASE}" fetch @com_google_protobuf_protoc_linux_aarch_64//:all

  bazel --output_base="${OUTPUT_BASE}" fetch @gperftools//:all

  # Fetch luajit2 explicitly - needed for s390x/ppc64le builds
  # bazel --output_base="${OUTPUT_BASE}" fetch @luajit2//:all || true

  # Fetch all the rest and check everything using "build --nobuild "option
  # Note: The envoy repository is automatically patched via patches = [...] in WORKSPACE
  for config in x86_64 aarch64 s390x; do
    bazel --output_base="${OUTPUT_BASE}" build --nobuild --keep_going --config="${config}" //... || true
  done
  # ppc pass: no CC toolchain for ppc64le on x86_64 hosts (toolchains_llvm is patched
  # post-vendor by patch_ppc64le). Suppress errors — deps are already fetched above.
  bazel --output_base="${OUTPUT_BASE}" build --nobuild --keep_going --config=ppc //... 2>/dev/null || true
}

function patch_s390x() {
  echo "Applying s390x build patches"
  for patch in "${ROOT_DIR}/ossm/patches/s390x/"*.patch; do
    echo "Applying ${patch}..."
    git apply --ignore-whitespace "${patch}"
  done
}

function patch_ppc64le() {
  echo "Applying ppc64le build patches"
  for patch in "${ROOT_DIR}/ossm/patches/ppc64le/"*.patch; do
    echo "Applying ${patch}..."
    git apply --ignore-whitespace "${patch}"
  done
}

function main() {
  validate
  init
  run_bazel
  copy_files
  patch_s390x
  patch_ppc64le

  echo
  echo "Done. Inspect the result with git status"
  echo
}

main
