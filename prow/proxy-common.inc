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

WD=$(dirname $0)
WD=$(cd $WD; pwd)
ROOT=$(dirname $WD)

# Exit immediately for non zero status
set -e
# Check unset variables
set -u
# Print commands
set -x

GOPATH=/home/prow/go
ROOT=/go/src

# Configure available resources and disable IPv6 tests.
export BAZEL_BUILD_ARGS="--local_ram_resources=12288 --local_cpu_resources=8 --verbose_failures --test_env=ENVOY_IP_TEST_VERSIONS=v4only --test_output=errors"

# e2e tests under //test/envoye2e/... use Bazel artifacts.
export BAZEL_OUT="$(bazel info output_path)/k8-fastbuild/bin"

# Use GCP service account when available.
if [[ -n "${GOOGLE_APPLICATION_CREDENTIALS:-}" ]]; then
  echo "Detected GOOGLE_APPLICATION_CREDENTIALS, activating..." >&2
  gcloud auth activate-service-account --key-file="${GOOGLE_APPLICATION_CREDENTIALS}"

  # Use RBE when logged in.
  BAZEL_BUILD_RBE_INSTANCE="${BAZEL_BUILD_RBE_INSTANCE:-projects/istio-testing/instances/default_instance}"
  BAZEL_BUILD_RBE_JOBS="${BAZEL_BUILD_RBE_JOBS:-200}"
  if [[ -n "${BAZEL_BUILD_RBE_INSTANCE}" ]]; then
    echo "Using RBE: ${BAZEL_BUILD_RBE_INSTANCE}"
    export BAZEL_BUILD_ARGS="${BAZEL_BUILD_ARGS} --google_credentials=${GOOGLE_APPLICATION_CREDENTIALS} --config=remote-clang-libc++ --config=remote-ci --remote_instance_name=${BAZEL_BUILD_RBE_INSTANCE} --jobs=${BAZEL_BUILD_RBE_JOBS}"
  fi
fi
