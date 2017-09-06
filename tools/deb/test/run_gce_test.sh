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
# Requires gcloud and access to a cluster that runs k8s. Will create a VM, install sidecar and
# run tests.

# TODO: alternative is user calling
# gcloud config set compute/zone us-central1-a
# gcloud config set core/project costin-raw

# To run the script, needs to override the following env with the current user.
export PROJECT=${PROJECT:-$(whoami)-raw}

export ISTIO_ZONE=${ISTIO_ZONE:-us-central1-a}

# Name of the k8s cluster running istio control plane. Used to find the service CIDR
K8SCLUSTER=${K8SCLUSTER:-raw}

TESTVM=${TESTVM:-testvm}

# Assuming the script is started from the proxy dir, under istio.io
ISTIO_IO=${ISTIO_IO:-${GOPATH}/src/istio.io}
PROXY_DIR=${ISTIO_IO}/proxy

# Used by functions that setup istio. Override if using a different hub in the build scripts.
TAG=${TAG:-$(whoami)}
HUB=${ISTIO_HUB:-gcr.io/istio-testing}

source $PROXY_DIR/tools/deb/test/istio_common.sh

# TODO: extend the script to use Vagrant+minikube or other ways to create the VM. The main
# issue is that we need the k8s and VM to be on same VPC - unless we run kubeapiserver+etcd
# standalone.


# Configure a service running on the VM.
# Params:
# - port of the service
# - service name (default to raw vm name)
# - IP of the rawvm (will get it from gcloud if not set)
# - type - defaults to http
#
function istioConfigService() {
  local NAME=${1:-}
  local PORT=${2:-}
  local IP=${3:-}
  local TYPE=${4:-http}

  # The 'name: http' is critical - without it the service is exposed as TCP

  cat << EOF | kubectl apply -f -
kind: Service
apiVersion: v1
metadata:
  name: $NAME
spec:
  ports:
    - protocol: TCP
      port: $PORT
      name: $TYPE

---

kind: Endpoints
apiVersion: v1
metadata:
  name: $NAME
subsets:
  - addresses:
      - ip: $IP
    ports:
      - port: $PORT
        name: $TYPE
EOF


}

function istioIngressRoutes() {

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

 LOCAL_IP=$(istioVMInternalIP $TESTVM)

 # Configure a service for the local nginx server, and add an ingress route
 istioConfigService $TESTVM 80 $LOCAL_IP

 # Bookinfo components on the VM
 istioConfigService ${TESTVM}-details 9080 $LOCAL_IP
 istioConfigService ${TESTVM}-productpage 9081 $LOCAL_IP
 istioConfigService ${TESTVM}-ratings 9082 $LOCAL_IP
 istioConfigService ${TESTVM}-mysql 3306 $LOCAL_IP

 # Prepare simple test to make sure istio-ingress can reach a http server on the VM
 istioRoute "/${TESTVM}/" ${TESTVM} 80
}

# Verify that cluster (istio-ingress) can reach the VM.
function testClusterToRawVM() {
  local INGRESS=$(ingressIP)

  # -f == fail, return != 0 if status code is not 200
  curl -f http://$INGRESS/${TESTVM}/

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
  istioVMDelete ${TESTVM}
}

if [[ ${1:-} == "setup" ]] ; then
  istioProvisionVM ${TESTVM}
elif [[ ${1:-} == "initVM" ]] ; then
  istioVMInit ${TESTVM}
elif [[ ${1:-} == "build" ]] ; then
  (cd $ISTIO_IO/proxy; tools/deb/test/build_all.sh)
elif [[ ${1:-} == "prepareCluster" ]] ; then
  istioPrepareCluster
elif [[ ${1:-} == "test" ]] ; then
  istioTestSetUp
  istioTest
elif [[ ${1:-} == "help" ]] ; then
  echo "$0 prepareCluster: provision an existing VM using the current build"
  echo "$0 initVM: create a test VM"
  echo "$0 setup: provision an existing VM using the current build"
  echo "$0 test: run tests"
  echo "$0 : create or reset VM, provision and run tests"
elif [[ ${1:-} == "env" ]] ; then
  echo "test environment variables enabled"
else
  # By default reset or create the VM, and do all steps. The VM will be left around until
  # next run, for debugging or other tests - next run will reset it (faster than create).
  istioVMInit ${TESTVM}

  istioPrepareCluster
  istioProvisionVM ${TESTVM}
  istioProvisionTestWorker ${TESTVM}
  istioTestSetUp
  istioTest

fi

