#!/usr/bin/env bash

set -o pipefail -o errexit -o nounset

readonly IMAGE_ATTESTER="$1"

# Run the cosign_attest target and check that it fails with the expected error message.
if ! "$IMAGE_ATTESTER" &> output.txt; then
  cat output.txt
  grep "ERROR: repository not set. Please pass --repository flag or set the 'repository' attribute in the rule." output.txt
else
  echo "Expected cosign_attest to fail, but it succeeded."
  exit 1
fi
