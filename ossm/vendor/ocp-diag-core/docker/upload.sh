#!/bin/bash

# upload.sh rebuilds the docker image, uploading it to the Google Container
# Registry at: gcr.io/ocpdiag-kokoro/ocpdiag-build.
#
# Be aware that this will immediately affect any Radial CI builds that use the
# docker image.
#
# You'll need write permissions to the 'ocpdiag-kokoro' GCP project to call this.

docker buildx build --push --platform linux/arm64,linux/amd64 -t gcr.io/ocpdiag-kokoro/ocpdiag-build .
