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

# To run the script, needs to override the following env with the current user.
export PROJECT=${PROJECT:-$(whoami)-raw}

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

source ${ISTIO_IO}/proxy/tools/deb/test/istio_common.sh

# Run a command in a VM.
function istioRun() {
  local NAME=$1
  local CMD=$2

  gcloud compute ssh --project $PROJECT --zone $ISTIO_ZONE $NAME --command "$CMD"
}

# Copy files to the VM
function istioCopy() {
  # TODO: based on some env variable, use different commands for other clusters or for testing with
  # bare-metal machines.
  local NAME=$1
  shift
  local FILES=$*

  gcloud compute scp --recurse --project $PROJECT --zone $ISTIO_ZONE $FILES ${NAME}:
}


# Create the raw VM.
function istioVMInit() {
  # TODO: check if it exists and do "reset", to speed up the script.
  local NAME=$1
  local IMAGE=${2:-debian-9-stretch-v20170816}
  local IMAGE_PROJECT=${3:-debian-cloud}

  gcloud compute --project $PROJECT instances  describe $NAME  --zone ${ISTIO_ZONE} >/dev/null 2>/dev/null
  if [[ $? == 0 ]] ; then

    gcloud compute --project $PROJECT \
     instances delete $NAME \
     --zone $ISTIO_ZONE \

  fi

  gcloud compute --project $PROJECT \
     instances create $NAME \
     --zone $ISTIO_ZONE \
     --machine-type "n1-standard-1" \
     --subnet default \
     --can-ip-forward \
     --scopes "https://www.googleapis.com/auth/cloud-platform" \
     --tags "http-server","https-server" \
     --image $IMAGE \
     --image-project $IMAGE_PROJECT \
     --boot-disk-size "10" \
     --boot-disk-type "pd-standard" \
     --boot-disk-device-name "debtest"

  # Allow access to the VM on port 80 and 9411 (where we run services)
  gcloud compute  --project $PROJECT firewall-rules create allow-external  --allow tcp:22,tcp:80,tcp:443,tcp:9411,udp:5228,icmp  --source-ranges 0.0.0.0/0


  # Wait for machine to start up ssh
  for i in {1..10}
  do
    istioRun $NAME 'echo hi' >/dev/null 2>/dev/null
    if [[ $? -ne 0 ]] ; then
        echo Waiting for startup $?
        sleep 5
    else
        break
    fi
  done

}


function istioVMDelete() {
  local NAME=${1:-$TESTVM}
  gcloud compute -q  instances delete $NAME --zone $ISTIO_ZONE --project $PROJECT
}

# Helper to get the external IP of a raw VM
function istioVMExternalIP() {
  local NAME=${1:-$TESTVM}
  gcloud compute --project $PROJECT instances describe $NAME --zone $ISTIO_ZONE --format='value(networkInterfaces[0].accessConfigs[0].natIP)'
}

function istioVMInternalIP() {
  local NAME=${1:-$TESTVM}
  gcloud compute --project $PROJECT instances describe $NAME  --zone $ISTIO_ZONE --format='value(networkInterfaces[0].networkIP)'
}

# Helper to use the raw istio config templates directly.
function istioTmpl() {
  local FILE=$1

  local SED="s,{PROXY_HUB},$HUB,; s,{PROXY_TAG},$TAG,; "
  SED="$SED s,{MIXER_HUB},$HUB,; s,{MIXER_TAG},$TAG,; "
  SED="$SED s,{PILOT_HUB},$HUB,; s,{PILOT_TAG},$TAG,; "
  SED="$SED s,{CA_HUB},$HUB,; s,{CA_TAG},$TAG,; "
}

function istioProvisionVM() {
 NAME=${1:-$TESTVM}

 local SA=${2:-istio.default}

  kubectl get secret $SA -o jsonpath='{.data.cert-chain\.pem}' |base64 -d  > cert-chain.pem
  kubectl get secret $SA -o jsonpath='{.data.root-cert\.pem}' |base64 -d  > root-cert.pem
  kubectl get secret $SA -o jsonpath='{.data.key\.pem}' |base64 -d  > key.pem

  istioRun $NAME "sudo rm -f istio-*.deb machine_setup.sh"

 # Copy deb, helper and config files
 # Reviews not copied - VMs don't support labels yet.
 istioCopy $NAME \
   kubedns \
   *.pem \
   cluster.env \
   tools/deb/test/machine_setup.sh \
   $ISTIO_IO/proxy/bazel-bin/tools/deb/istio-proxy-envoy.deb \
   $ISTIO_IO/pilot/bazel-bin/tools/deb/istio-agent.deb \
   $ISTIO_IO/auth/bazel-bin/tools/deb/istio-node-agent.deb \
   $ISTIO_IO/istio/samples/apps/bookinfo/src/details \
   $ISTIO_IO/istio/samples/apps/bookinfo/src/mysql \
   $ISTIO_IO/istio/samples/apps/bookinfo/src/productpage \
   $ISTIO_IO/istio/samples/apps/bookinfo/src/ratings


 istioRun $NAME "sudo bash -c -x ./machine_setup.sh $NAME"

}

function ingressIP() {
  kubectl get service istio-ingress -o jsonpath='{.status.loadBalancer.ingress[0].ip}'
}

function setUp() {
cat <<EOF | kubectl apply -f -
apiVersion: extensions/v1beta1
kind: Ingress
metadata:
  name: zipkin-ingress
  annotations:
    kubernetes.io/ingress.class: istio
spec:
  rules:
  - http:
      paths:
      - path: /zipkin/.*
        backend:
          serviceName: zipkin
          servicePort: 9411
EOF

cat <<EOF | kubectl apply -f -
apiVersion: extensions/v1beta1
kind: Ingress
metadata:
  name: prom-ingress
  annotations:
    kubernetes.io/ingress.class: istio
spec:
  rules:
  - http:
      paths:
      - path: /zipkin/.*
        backend:
          serviceName: zipkin
          servicePort: 9411
EOF


    cat <<EOF | kubectl apply -f -
apiVersion: extensions/v1beta1
kind: Ingress
metadata:
  name: grafana-ingress
  annotations:
    kubernetes.io/ingress.class: istio
spec:
  rules:
  - http:
      paths:
      - path: /grafana/.*
        backend:
          serviceName: grafana
          servicePort: 3000
      - path: /public/.*
        backend:
          serviceName: grafana
          servicePort: 3000
      - path: /dashboard/.*
        backend:
          serviceName: grafana
          servicePort: 3000
      - path: /api/.*
        backend:
          serviceName: grafana
          servicePort: 3000
EOF

}

# Add a URL route to a raw VM service
function istioRoute() {
  local ROUTE=${1:-}
  local SERVICE=${2:-}
  local PORT=${3:-}

cat <<EOF | kubectl apply -f -
apiVersion: extensions/v1beta1
kind: Ingress
metadata:
  name: rawvm-ingress
  annotations:
    kubernetes.io/ingress.class: istio
spec:
  rules:
  - http:
      paths:
      - path: ${ROUTE}
        backend:
          serviceName: ${SERVICE}
          servicePort: ${PORT}
EOF

}


function istioTestSetUp() {
>>>>>>> 26ab13f903004e410cb8d274a4eaa7d7a41215c1

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

<<<<<<< HEAD
if [[ ${1:-} == "setupVM" ]] ; then
  istioProvisionVM ${VMNAME}
elif [[ ${1:-} == "initVM" ]] ; then
  istioVMInit ${VMNAME}
=======
if [[ ${1:-} == "setup" ]] ; then
  istioProvisionVM ${TESTVM}
elif [[ ${1:-} == "initVM" ]] ; then
  istioVMInit ${TESTVM}
>>>>>>> 26ab13f903004e410cb8d274a4eaa7d7a41215c1
elif [[ ${1:-} == "build" ]] ; then
  (cd $ISTIO_IO/proxy; tools/deb/test/build_all.sh)
elif [[ ${1:-} == "prepareCluster" ]] ; then
  istioPrepareCluster
elif [[ ${1:-} == "test" ]] ; then
  istioTestSetUp
  istioTest
elif [[ ${1:-} == "help" ]] ; then
<<<<<<< HEAD
  echo "$0 prepareCluster: provision an existing Istio cluster for VM use"
  echo "$0 initVM: create a test VM"
  echo "$0 setupVM: provision an existing VM using the current build"
=======
  echo "$0 prepareCluster: provision an existing VM using the current build"
  echo "$0 initVM: create a test VM"
  echo "$0 setup: provision an existing VM using the current build"
>>>>>>> 26ab13f903004e410cb8d274a4eaa7d7a41215c1
  echo "$0 test: run tests"
  echo "$0 : create or reset VM, provision and run tests"
elif [[ ${1:-} == "env" ]] ; then
  echo "test environment variables enabled"
else
  # By default reset or create the VM, and do all steps. The VM will be left around until
  # next run, for debugging or other tests - next run will reset it (faster than create).
<<<<<<< HEAD
  istioVMInit ${VMNAME}

  istioPrepareCluster
  istioProvisionVM ${VMNAME}
  istioProvisionTestWorker ${VMNAME}
=======
  istioVMInit ${TESTVM}

  istioPrepareCluster
  istioProvisionVM ${TESTVM}
  istioProvisionTestWorker ${TESTVM}
>>>>>>> 26ab13f903004e410cb8d274a4eaa7d7a41215c1
  istioTestSetUp
  istioTest

fi

