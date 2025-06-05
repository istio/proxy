#!/bin/bash

# test.sh locally builds the docker image and then runs an interactive
# terminal inside the docker image. You can set the OCPDIAG_DIR environment
# variable to the location of a local pull of OCPDiag or copybara output for
# convenience. You can optionally input an architecture, but if none is
# specified then it will default to amd64.
#
# Example usage:
#   ./test.sh
#   ./test.sh arm64

arch="amd64"
if [[ $# -eq 1 ]]; then
  arch=$1
fi

docker buildx build \
  --push \
  --platform "linux/${arch}" \
  -t gcr.io/ocpdiag-kokoro/ocpdiag-build-staging .
docker run \
  --rm \
  --platform "linux/${arch}" \
  --volume "${OCPDIAG_DIR}:/workspace/git/ocpdiag" \
  -it gcr.io/ocpdiag-kokoro/ocpdiag-build-staging \
  /bin/bash
