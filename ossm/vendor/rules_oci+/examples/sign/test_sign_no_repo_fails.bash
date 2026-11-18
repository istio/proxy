#!/usr/bin/env bash

set -o pipefail -o errexit -o nounset

readonly IMAGE_SIGNER="$1"

# Run the cosign_sign target and check that it fails with the expected error message.
if ! "$IMAGE_SIGNER" &> output.txt; then
  cat output.txt
  grep "ERROR: repository not set. Please pass --repository flag or set the 'repository' attribute in the rule." output.txt
else
  echo "Expected cosign_sign to fail, but it succeeded."
  exit 1
fi
