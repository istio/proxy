#!/usr/bin/env bash

set -o pipefail -o errexit -o nounset

readonly PUSH_IMAGE_WO_REPOSITORY="$1"

# Run the oci_push target and check that it fails with an error message.
if ! "$PUSH_IMAGE_WO_REPOSITORY" &> output.txt; then
  cat output.txt
  grep "ERROR: repository not set. Please pass --repository flag." output.txt
else
  echo "Expected oci_push to fail, but it succeeded."
  exit 1
fi
