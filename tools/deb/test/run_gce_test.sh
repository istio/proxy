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

# Integration test for Raw VM, using GCE and GKE. Will setup all components, preparing
# for running the tests or demo.

# Requirements:
# - gcloud, with all required priviledges ( cluster admin )
# - a GKE cluster with Istio installed

# Before running the script, you need to configure the project and zone, for example:
# gcloud config set core/project costin-raw
# gcloud config set compute/zone us-central1-a
#
# Alternative: set PROJECT and ISTIO_ZONE environment variables.


# TODO: create the GKE cluster and install istio, for a fully reproductible experience.

# Name of the k8s cluster running istio control plane. Used to find the service CIDR
K8SCLUSTER=${K8SCLUSTER:-raw}
VMNAME=${TESTVM:-${VMNAME:-testvm}}

# Base directory where Istio files are checked out.
# Defaults to GOPATH, which defaults to ~/go
ISTIO_BASE=${ISTIO_BASE:-${GOPATH:-${HOME}/go}}

# Used by functions that setup istio. Override if using a different hub in the build scripts.
TAG=${TAG:-$(whoami)}
HUB=${ISTIO_HUB:-gcr.io/istio-testing}

# Assuming the script is started from the proxy dir, under istio.io
ISTIO_IO=${ISTIO_IO:-${ISTIO_BASE}/src/istio.io}

source ${ISTIO_IO}/proxy/tools/deb/test/istio_raw.sh

# TODO: extend the script to use Vagrant+minikube or other ways to create the VM. The main
# issue is that we need the k8s and VM to be on same VPC - unless we run kubeapiserver+etcd
# standalone.



function istioTestSetUp() {

 LOCAL_IP=$(istioVMInternalIP $VMNAME)

 # Configure a service for the local nginx server, and add an ingress route
 istioConfigService $TESTVM 80 $LOCAL_IP

 # Bookinfo components on the VM
 istioConfigService ${VMNAME}-details 9080 $LOCAL_IP
 istioConfigService ${VMNAME}-productpage 9081 $LOCAL_IP
 istioConfigService ${VMNAME}-ratings 9082 $LOCAL_IP
 istioConfigService ${VMNAME}-mysql 3306 $LOCAL_IP

 # Prepare simple test to make sure istio-ingress can reach a http server on the VM
 istioRoute "/${VMNAME}/" ${VMNAME} 80
}

# Verify that cluster (istio-ingress) can reach the VM.
function testClusterToRawVM() {
  local INGRESS=$(ingressIP)

  # -f == fail, return != 0 if status code is not 200
  curl -f http://$INGRESS/${VMNAME}/

  echo $?
}

# Verify that the VM can reach the cluster. Use the local http server running on the VM.
function testRawVMToCluster() {
  local RAWVM=$(istioVMExternalIP)

  # -f == fail, return != 0 if status code is not 200
  curl -f http://${RAWVM}:9411/zipkin/

  echo $?
}

function istioTest() {
  testRawVMToCluster
  testClusterToRawVM
}

function tearDown() {
  # TODO: it is also possible to reset the VM, may be faster
  istioVMDelete ${VMNAME}
}

if [[ ${1:-} == "setupVM" ]] ; then
  istioProvisionVM ${VMNAME}
elif [[ ${1:-} == "initVM" ]] ; then
  istioVMInit ${VMNAME}
elif [[ ${1:-} == "build" ]] ; then
  (cd $ISTIO_IO/proxy; tools/deb/test/build_all.sh)
elif [[ ${1:-} == "prepareCluster" ]] ; then
  istioPrepareCluster
elif [[ ${1:-} == "test" ]] ; then
  istioTestSetUp
  istioTest
elif [[ ${1:-} == "help" ]] ; then
  echo "$0 prepareCluster: provision an existing Istio cluster for VM use"
  echo "$0 initVM: create a test VM"
  echo "$0 setupVM: provision an existing VM using the current build"
  echo "$0 test: run tests"
  echo "$0 : create or reset VM, provision and run tests"
else
  # By default reset or create the VM, and do all steps. The VM will be left around until
  # next run, for debugging or other tests - next run will reset it (faster than create).
  istioVMInit ${VMNAME}

  istioPrepareCluster
  istioProvisionVM ${VMNAME}
  istioProvisionTestWorker ${VMNAME}
  istioTestSetUp
  istioTest

fi

