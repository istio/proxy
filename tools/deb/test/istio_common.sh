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
#
################################################################################

# Function useful for setting up and testing istio on raw VM.
# Can be sourced from a shell, and each function used independently
# Requires GOPATH to be set to the working root istio directory.


# Build debian and binaries for all components we'll test on the VM
# Will checkout or update from master, in the typical layout.
function istio_build_all() {
  mkdir -p $GOPATH/src/istio.io

  for sub in pilot istio mixer auth proxy; do
    if [[ -d $GOPATH/src/istio.io/$sub ]]; then
      (cd $GOPATH/src/istio.io/$sub; git pull origin master)
    else
      (cd $GOPATH/src/istio.io; git clone https://github.com/istio/$sub)
    fi
  done

  # Note: components may still use old SHA - but the test will build the binaries from master
  # from each component, to make sure we don't test old code.
  pushd $GOPATH/src/istio.io/pilot
  bazel build ...
  ./bin/init.sh
  popd

  (cd $GOPATH/src/istio.io/mixer; bazel build ...)

  (cd $GOPATH/src/istio.io/proxy; bazel build tools/deb/...)

  (cd $GOPATH/src/istio.io/auth; bazel build ...)

}

# Build docker images for istio. Intended to test backward compat.
function istio_build_docker() {
  local TAG=${1:-$(whoami)}
  # Will create a local docker image gcr.io/istio-testing/envoy-debug:USERNAME

  (cd $GOPATH/src/istio.io/proxy; TAG=$TAG ./scripts/release-docker debug)
  gcloud docker -- push gcr.io/istio-testing/envoy-debug:$TAG

  (cd $GOPATH/src/istio.io/pilot; ./bin/push-docker -tag $TAG)

  (cd $GOPATH/src/istio.io/auth; ./bin/push-docker.sh -t $TAG -h gcr.io/istio-testing)

  (cd $GOPATH/src/istio.io/mixer; ./bin/publish-docker-images.sh -h gcr.io/istio-testing -t $TAG)

}

# Update k8s cluster with current images.
function istio_k8s_update() {
  local TAG=${1:-$(whoami)}

  for y in *.yaml; do
    kubectl apply -f $y
  done
}


# Show the branch and status of each istio repo
function istio_status() {
  cd $GOPATH/src/istio.io

  for sub in pilot istio mixer auth proxy; do
     echo -e "\n\n$sub\n"
     (cd $GOPATH/src/istio.io/$sub; git branch; git status)
  done
}

# Get an istio service account secret, extract it to files to be provisioned on a raw VM
function istio_provision_certs() {
  local SA=${1:-istio.default}

  kubectl get secret $SA -o jsonpath='{.data.cert-chain\.pem}' |base64 -d  > cert-chain.pem
  kubectl get secret $SA -o jsonpath='{.data.root-cert\.pem}' |base64 -d  > root-cert.pem
  kubectl get secret $SA -o jsonpath='{.data.key\.pem}' |base64 -d  > key.pem

}

# Copy files to the VM
function istioVMCopy() {
  # TODO: based on some env variable, use different commands for other clusters or for testing with
  # bare-metal machines.
  local NAME=$1
  shift
  local FILES=$*
  local ISTIO_ZONE=${ISTIO_ZONE:-us-west1-c}


  gcloud compute scp  --zone $ISTIO_ZONE --recurse $FILES ${NAME}:
}
