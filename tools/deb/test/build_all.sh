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

# For e2e tests, build pilot, mixer, istio from head.
if [ -z $GOPATH ]; then
  echo "GOPATH env not set, will use ~/go"
  GOPATH=~/go
fi

if [ -z $BRANCH ]; then
  BRANCH=rawvm-demo-0-2-2
  echo "BRANCH not set will use $BRANCH for new clone, unchanged for pull"
fi

# Build debian and binaries for all components we'll test on the VM
# Will checkout mixer, pilot and proxy in the expected locations/
function build_all() {
  mkdir -p $GOPATH/src/istio.io


  if [[ -d $GOPATH/src/istio.io/pilot ]]; then
    (cd $GOPATH/src/istio.io/pilot; git pull)
  else
    (cd $GOPATH/src/istio.io; git clone https://github.com/istio/pilot -b $BRANCH)
  fi

  if [[ -d $GOPATH/src/istio.io/istio ]]; then
    (cd $GOPATH/src/istio.io/istio; git pull)
  else
    (cd $GOPATH/src/istio.io; git clone https://github.com/istio/istio -b $BRANCH)
  fi

  if [[ -d $GOPATH/src/istio.io/mixer ]]; then
    (cd $GOPATH/src/istio.io/mixer; git pull)
  else
    (cd $GOPATH/src/istio.io; git clone https://github.com/istio/mixer -b $BRANCH)
  fi

  if [[ -d $GOPATH/src/istio.io/proxy ]]; then
    (cd $GOPATH/src/istio.io/proxy; git pull)
  else
    (cd $GOPATH/src/istio.io; git clone https://github.com/istio/proxy -b $BRANCH)
  fi

  pushd $GOPATH/src/istio.io/pilot
  bazel build ...
  ./bin/init.sh
  popd

  (cd $GOPATH/src/istio.io/mixer; bazel build ...)
  bazel build tools/deb/...

}

build_all
