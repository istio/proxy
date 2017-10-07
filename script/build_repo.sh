#!/usr/bin/env bash

# Script to build istio proxy, using a repo manifest to manage git dependencies.
# All deps will be pulled from head (or the branch specified in the manifest).

WS=${PROXY_SRC:-`pwd`}

# Download the dependencies, using the repo manifest
function init_repo() {
    BASE=${ISTIO_REPO:-https://github.com/istio/proxy.git}

    pushd $WS
    if [ ! -f $WS/build.sh ]; then
       git clone $BASE .
    fi

    if [ ! -f bin/repo ]; then
      mkdir -p bin
      curl https://storage.googleapis.com/git-repo-downloads/repo > bin/repo
      chmod a+x bin/repo
    fi

    if [ ! -f .repo ]; then
      echo y | bin/repo init -u http://github.com/costinm/istio-proxy-repo
    fi

    bin/repo sync -c
    popd
}

function build() {
    bazel build src/envoy/mixer:envoy
    bazel build tools/deb:istio-proxy

    (cd go/src/istio.io/pilot; bazel build cmd/pilot-agent:pilot-agent tools/deb/...)

    # Docker images will be build by separate script or cloud builder
    # This just copies files
    prepare_docker
}

# Copy the docker files, preparing for docker build
function prepare_docker() {
  BAZEL_TARGET_DIR="bazel-bin/src/envoy/mixer"

  cp tools/deb/istio-iptables.sh ${BAZEL_TARGET_DIR}
  cp tools/deb/istio-start.sh ${BAZEL_TARGET_DIR}
  cp tools/deb/envoy.json ${BAZEL_TARGET_DIR}
  cp docker/proxy-* ${BAZEL_TARGET_DIR}
  cp docker/Dockerfile.* ${BAZEL_TARGET_DIR}/
}


if [[ ${1:-} == "sync" ]] ; then
   init_repo

elif [[ ${1:-} == "build" ]] ; then
   build
fi
