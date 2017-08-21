#!/bin/bash

# Run a local pilot, local envoy, [local mixer], using a hand-crafted config.

# Assumes proxy is in a src/proxy directory (as created by repo). Override WS env to use a different base dir
# or make sure GOPATH is set to the base of the 'go' workspace
WS=${WS:-$(pwd)/../..}
export GOPATH=${GOPATH:-$WS/go}
LOG_DIR=${LOG_DIR:-test.logs}
PILOT=${PILOT:-${GOPATH}/src/istio.io/pilot}

# Build debian and binaries for all components we'll test on the VM
# Will checkout mixer, pilot and proxy in the expected locations/
function build_all() {
  mkdir -p $WS/go/src/istio.io


  if [[ -d $GOPATH/src/istio.io/pilot ]]; then
    (cd $GOPATH/src/istio.io/pilot; git pull upstream master)
  else
    #(cd $GOPATH/src/istio.io; git clone https://github.com/istio/pilot)
    (cd $GOPATH/src/istio.io; git clone https://github.com/costinm/pilot -b deb)
  fi

  if [[ -d $GOPATH/src/istio.io/mixer ]]; then
    (cd $GOPATH/src/istio.io/mixer; git pull upstream master)
  else
    (cd $GOPATH/src/istio.io; git clone https://github.com/istio/mixer)
  fi

  pushd $GOPATH/src/istio.io/pilot
  bazel build ...
  ./bin/init.sh
  popd

  (cd $GOPATH/src/istio.io/mixer; bazel build ...)
  bazel build tools/deb/...

}

function kill_all() {
  kill -9 $(cat $LOG_DIR/pilot.pid)
  kill -9 $(cat $LOG_DIR/mixer.pid)
  kill -9 $(cat $LOG_DIR/envoy.pid)

}

function start_all() {
  POD_NAME=pilot POD_NAMESPACE=default  ${PILOT}/bazel-bin/cmd/pilot-discovery/pilot-discovery discovery -n default --kubeconfig ~/.kube/config &
  echo $! > $LOG_DIR/pilot.pid

  ${GOPATH}/src/istio.io/mixer/bazel-bin/main server --configStoreURL=fs://ws/istio/istio-xp/mixer -v=2 --logtostderr &
  echo $! > $LOG_DIR/mixer.pid

  # 'lds' disabled, so we can use manual config.
  bazel-bin/src/envoy/mixer/envoy -c tools/deb/test/envoy_local.json --restart-epoch 0 --drain-time-s 2 --parent-shutdown-time-s 3 --service-cluster istio-proxy --service-node sidecar~172.17.0.2~mysvc.~svc.cluster.local &
  echo $! > $LOG_DIR/envoy.pid
}

build_all
kill_all
start_all
