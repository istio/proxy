#!/bin/bash

set -exo pipefail

DIR=$(cd "$(dirname "$0")" ; pwd -P)

# shellcheck disable=SC1091
source "${DIR}/common.sh"

GCS_PROJECT=${GCS_PROJECT:-maistra-prow-testing}
ARTIFACTS_GCS_PATH=${ARTIFACTS_GCS_PATH:-gs://maistra-prow-testing/proxy}

gcloud auth activate-service-account --key-file="${GOOGLE_APPLICATION_CREDENTIALS}"
gcloud config set project "${GCS_PROJECT}"

# Build Envoy
bazel_build envoy_tar

# Copy artifacts to GCS
SHA="$(git rev-parse --verify HEAD)"

if [[ "$(uname -m)" == "aarch64" ]]; then
  ARCH_SUFFIX="-arm64"
else
  ARCH_SUFFIX=""
fi

gsutil cp bazel-bin/envoy_tar.tar.gz "${ARTIFACTS_GCS_PATH}/envoy-alpha-${SHA}${ARCH_SUFFIX}.tar.gz"
