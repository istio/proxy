#!/usr/bin/env bash

# Script to build istio proxy.
WS=${PROXY_SRC:-`pwd`}

# Download the source files.
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
      bin/repo init -u http://github.com/costinm/istio-proxy-repo
    fi

    bin/repo sync -c
    popd
}



init_repo