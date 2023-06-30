#!/bin/bash
#
# Copyright 2017 Istio Authors. All Rights Reserved.
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

WD=$(dirname "$0")
WD=$(cd "$WD" || exit 1; pwd)
ROOT=$(dirname "$WD")
WORKSPACE="${ROOT}/WORKSPACE"

# Exit immediately for non zero status
set -e
# Check unset variables
set -u
# Print commands
set -x

# shellcheck disable=SC2034
GOPATH=/home/prow/go
ROOT=/go/src

# Configure available resources and disable IPv6 tests.
export BAZEL_BUILD_ARGS="--verbose_failures --test_env=ENVOY_IP_TEST_VERSIONS=v4only --test_output=errors"

# Override envoy.
if [[ "${ENVOY_REPOSITORY:-}" && "${ENVOY_PREFIX:-}" ]]; then
  # Legacy path, keep around for a few releases to allow folks to migrate
  # The other path is preferred as it uses an API intended for programmatic access and allows fine-grained access token usage.
  TMP_DIR=$(mktemp -d -t envoy-XXXXXXXXXX)
  trap 'rm -rf ${TMP_DIR:?}' EXIT
  ENVOY_SHA="${ENVOY_SHA:-$(grep -Pom1 "^ENVOY_SHA = \"\K[a-zA-Z0-9]{40}" "$WORKSPACE")}"
  BAZEL_BUILD_ARGS="${BAZEL_BUILD_ARGS} --override_repository=envoy=${TMP_DIR}/${ENVOY_PREFIX}-${ENVOY_SHA}"
  curl -nsSfL "${ENVOY_REPOSITORY}/archive/${ENVOY_SHA}.tar.gz" | tar -C "${TMP_DIR}" -xz
elif [[ "${ENVOY_REPOSITORY:-}" ]]; then
  TMP_DIR=$(mktemp -d -t envoy-XXXXXXXXXX)
  trap 'rm -rf ${TMP_DIR:?}' EXIT
  ENVOY_SHA="${ENVOY_SHA:-$(grep -Pom1 "^ENVOY_SHA = \"\K[a-zA-Z0-9]{40}" "$WORKSPACE")}"
  BAZEL_BUILD_ARGS="${BAZEL_BUILD_ARGS} --override_repository=envoy=${TMP_DIR}"
  # User passes a URl like https://github.com/foo/bar, we translate it to https://api.github.com/foo/bar/repos.
  api_url=${ENVOY_REPOSITORY/https:\/\/github.com/https:\/\/api.github.com/repos}
  # Contents in the tarball will be in a folder, so --strip=1 it so we don't have to deal with the changing name
  curl -nsSfL "${api_url}/tarball/${ENVOY_SHA}" | tar -C "${TMP_DIR}" -xz --strip=1
fi

# e2e tests under //test/envoye2e/... use Bazel artifacts.
# shellcheck disable=SC2086
BAZEL_OUT="$(bazel info ${BAZEL_BUILD_ARGS} output_path)/k8-fastbuild/bin"
export BAZEL_OUT

# Disable RBE execution because the tool chain used by RBE is too new.
export BAZEL_BUILD_RBE_JOBS="${BAZEL_BUILD_RBE_JOBS:-0}"

BAZEL_BUILD_RBE_INSTANCE="${BAZEL_BUILD_RBE_INSTANCE-projects/istio-testing/instances/default_instance}"
BAZEL_BUILD_RBE_CACHE="${BAZEL_BUILD_RBE_CACHE-grpcs://remotebuildexecution.googleapis.com}"

CREDS="google_default_credentials"
# Use GCP service account when available.
if [[ -n "${GOOGLE_APPLICATION_CREDENTIALS:-}" ]]; then
  echo "Using legacy GOOGLE_APPLICATION_CREDENTIALS. Move to workload identity!" >&2
  gcloud auth activate-service-account --key-file="${GOOGLE_APPLICATION_CREDENTIALS}"
  CREDS="google_credentials=${GOOGLE_APPLICATION_CREDENTIALS}"
fi

BAZEL_BUILD_RBE_JOBS="${BAZEL_BUILD_RBE_JOBS:-200}"
if [[ -n "${BAZEL_BUILD_RBE_INSTANCE}" ]]; then
  if [[ "${BAZEL_BUILD_RBE_JOBS}" -gt 0 ]]; then
    echo "Using RBE: ${BAZEL_BUILD_RBE_INSTANCE} (execute)"
    export BAZEL_BUILD_ARGS="${BAZEL_BUILD_ARGS} --${CREDS} --config=remote-clang-libc++ --config=remote-ci --remote_instance_name=${BAZEL_BUILD_RBE_INSTANCE} --jobs=${BAZEL_BUILD_RBE_JOBS}"
  elif [[ -n "${BAZEL_BUILD_RBE_CACHE}" ]]; then
    echo "Using RBE: ${BAZEL_BUILD_RBE_INSTANCE} (cache)"
    export BAZEL_BUILD_ARGS="${BAZEL_BUILD_ARGS} --${CREDS} --remote_instance_name=${BAZEL_BUILD_RBE_INSTANCE} --remote_cache=${BAZEL_BUILD_RBE_CACHE}"
  fi
fi
