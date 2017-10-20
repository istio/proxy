#!/usr/bin/env bash

# Script to build istio proxy, using a repo manifest to manage git dependencies.
# All deps will be pulled from head (or the branch specified in the manifest).

WS=${PROXY_SRC:-`pwd`}
PROXY=.
DEPS=./vendor

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

# Populate the pre-generated files - using the canonical bazle build.
# In future this can be generated directly with protoc.
function genUpdate() {
    mkdir -p $WS/genfiles
    cp -a -f $PROXY/bazel-genfiles/external/mixerapi_git/mixer/v1 $WS/genfiles/mixer

    #rm -rf $WS/genfiles/src/lightstep/lightstep-tracer-common
    #cp -a $PROXY/bazel-genfiles/external/mixerapi_git/mixer/v1 $WS/genfiles/src/lightstep/

    cp -a -f $PROXY/bazel-genfiles/external/gogoproto_git/gogoproto $WS/genfiles

    cp -a -f $PROXY/bazel-genfiles/external/googleapis_git/google/rpc/ $WS/genfiles/google/

   cp -a -f $PROXY/bazel-genfiles/external/envoy_api/api  $WS/genfiles

    cp -a -f  $PROXY/bazel-genfiles/src/envoy/mixer  $WS/genfiles/src/envoy
    cp -a -f  $PROXY/bazel-genfiles/external/googleapis/google/api  /ws/istio-master/genfiles/google
    mkdir -p $WS/genfiles/common/filesystem/
    cp -f $WS/envoy/source/common/filesystem/inotify/watcher_impl.h $WS/genfiles/common/filesystem/

    cp -a -f  $PROXY/bazel-genfiles/external/envoy/source/common/ratelimit $WS/genfiles/source/common

   cp $PROXY/bazel-proxy/external/envoy_deps/thirdparty_build/include/lightstep_carrier.pb.h $WS/genfiles/
    cp $PROXY/bazel-proxy/external/envoy_deps/thirdparty_build/include/collector.pb.h $WS/genfiles/

    cp $PROXY/bazel-genfiles/external/mixerclient_git/src/global_dictionary.cc $WS/genfiles/

    PROTOC=$PROXY/bazel-out/host/bin/external/com_google_protobuf/protoc

    #pushd cmake-build-debug
    #make protoc
    #PROTOC=./vendor/protobuf/cmake/protoc
    #./vendor/protobuf/cmake/protoc  --proto_path=../vendor/lightstep/lightstep-tracer-common/:../vendor/protobuf/src --cpp_out=../genfiles/ ../vendor/lightstep/lightstep-tracer-common/lightstep_carrier.proto
    #./vendor/protobuf/cmake/protoc  --proto_path=../vendor/lightstep/lightstep-tracer-common/:../vendor/protobuf/src --cpp_out=../genfiles/ ../vendor/lightstep/lightstep-tracer-common/collector.proto
    #popd
    $PROTOC  --proto_path=$DEPS/lightstep/lightstep-tracer-common/:$DEPS/protobuf/src --cpp_out=genfiles/ $DEPS/lightstep/lightstep-tracer-common/lightstep_carrier.proto
    $PROTOC  --proto_path=$DEPS/lightstep/lightstep-tracer-common/:$DEPS/protobuf/src --cpp_out=genfiles/ $DEPS/lightstep/lightstep-tracer-common/collector.proto

}

function cmakeDeb() {
   mkdir -p cmake-build-debug
   cmake ..
   make envoy
   cd ..

   mkdir -p cmake-alpine-debug

   docker run --name build --rm -u $(id -u) -it \
     -v $PWD:/workspace -w /workspace -e USER=$USER \
     --entrypoint /bin/bash docker.io/costinm/istio-alpine-build \
     -c "cd cmake-alpine-debug; cmake .. -DUSE_MUSL:bool=ON ; make -j8 envoy"

   mkdir -p cmake-pi-debug
   mkdir -p prebuilts/linux-x86
   ln -s /opt/pi prebuilts/linux-x86
   docker run --name build --rm -u $(id -u) -it \
     -v $PWD:/workspace -w /workspace -e USER=$USER \
     --entrypoint /bin/bash docker.io/costinm/istio-pi-build \
     -c "cd cmake-android-debug; cmake -DCMAKE_TOOLCHAIN_FILE=../tools/cmake/pi.cmake ..; make -j8 envoy"

   mkdir -p cmake-android-debug
   docker run --name build --rm -u $(id -u) -it \
     -v $PWD:/workspace -w /workspace -e USER=$USER \
     --entrypoint /bin/bash docker.io/costinm/istio-android-build \
     -c "cd cmake-android-debug; cmake -DANDROID_STL=c++_static -DANDROID_TOOLCHAIN=clang -DANDROID_PLATFORM=android-26 -DANDROID_NDK=$ANDROID_NDK  -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake ..; make -j8 envoy"

}

if [[ ${1:-} == "sync" ]] ; then
   init_repo

elif [[ ${1:-} == "gen" ]] ; then
  genUpdate

elif [[ ${1:-} == "cmake" ]] ; then
   cmakeDeb

elif [[ ${1:-} == "build" ]] ; then
   build
fi
