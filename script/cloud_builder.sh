#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail
set -x

# Cloud Builder does not support running as a user
# Note the image used has a releng user, but we start as root
# and need to su into releng

RELENG_HOME='/home/releng'
WORKSPACE="${PWD}"

echo 'Downloading docker.io encoded config'
gsutil cp gs://istio-secrets/dockerhub_config.json.enc \
  "${RELENG_HOME}/config.json.enc"

echo 'Decoding docker.io config'
gcloud kms decrypt \
  --ciphertext-file="${RELENG_HOME}/config.json.enc" \
  --plaintext-file="${RELENG_HOME}/config.json" \
  --location=global \
  --keyring=Secrets \
  --key=DockerHub \
  --verbosity=info

echo 'Setting bazel.rc'
cp tools/bazel.rc.cloudbuilder "${HOME}/.bazelrc"

#echo "Changing ${WORKSPACE} ownership to releng"
#chown -R releng -R "${WORKSPACE}"

#su releng -c "./
script/release.sh ${@}
