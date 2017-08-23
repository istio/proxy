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

# To run the script, needs to override the following env with the current user.
export PROJECT=${PROJECT:-costin-istio}
export ACCOUNT=${ACCOUNT:-291514510799-compute@developer.gserviceaccount.com}
export ISTIO_REGION=${ISTIO_REGION:-us-west1}
export ISTIO_ZONE=${ISTIO_ZONE:-us-west1-c}

# Name of the k8s cluster running istio control plane. Used to find the service CIDR
K8SCLUSTER=${K8SCLUSTER:-istio-auth}

TESTVM=${TESTVM:-istiotestvm}

PROXY_DIR=$(pwd)
WS=${WS:-$(pwd)/../..}

# TODO: extend the script to use Vagrant+minikube or other ways to create the VM. The main
# issue is that we need the k8s and VM to be on same VPC - unless we run kubeapiserver+etcd
# standalone.

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

  gcloud compute scp --project $PROJECT --zone $ISTIO_ZONE $FILES ${NAME}:
}


# Create the raw VM.
function istioVMInit() {
  # TODO: check if it exists and do "reset", to speed up the script.
  local NAME=$1
  local IMAGE=${2:-debian-9-stretch-v20170717}
  gcloud compute --project $PROJECT instances create $NAME --zone $ISTIO_ZONE \
   --machine-type "n1-standard-1" \
   --subnet default \
   --can-ip-forward \
   --service-account $ACCOUNT \
   --scopes "https://www.googleapis.com/auth/cloud-platform" \
   --tags "http-server","https-server" \
   --image $IMAGE \
   --image-project "debian-cloud" \
   --boot-disk-size "10" \
   --boot-disk-type "pd-standard" \
   --boot-disk-device-name "debtest"
}


function istioVMDelete() {
  local NAME=$1
  gcloud compute -q --project $PROJECT instances delete $NAME --zone $ISTIO_ZONE
}

# Helper to get the external IP of a raw VM
function istioVMExternalIP() {
  local NAME=$1
  gcloud compute --project $PROJECT instances describe $NAME --format='value(networkInterfaces[0].accessConfigs[0].natIP)'
}

function istioVMInternalIP() {
  local NAME=$1
  gcloud compute --project $PROJECT instances describe $NAME  --format='value(networkInterfaces[0].networkIP)'
}

# Initialize the K8S cluster, generating config files for the raw VMs.
# Must be run once, will generate files in the CWD. The files must be installed on the VM.
# This assumes the recommended dnsmasq config option.
function istioPrepareCluster() {
cat <<EOF | kubectl apply -f -
apiVersion: v1
kind: Service
metadata:
  name: istio-pilot-ilb
  annotations:
    cloud.google.com/load-balancer-type: "internal"
  labels:
    istio: pilot
spec:
  type: LoadBalancer
  ports:
  - port: 8080
    protocol: TCP
  selector:
    istio: pilot
EOF
cat <<EOF | kubectl apply -n kube-system -f -
apiVersion: v1
kind: Service
metadata:
  name: dns-ilb
  annotations:
    cloud.google.com/load-balancer-type: "internal"
  labels:
    k8s-app: kube-dns
spec:
  type: LoadBalancer
  ports:
  - port: 53
    protocol: UDP
  - port: 53
    protocol: TCP
  selector:
    k8s-app: kube-dns
EOF

cat <<EOF | kubectl apply -f -
apiVersion: v1
kind: Service
metadata:
  name: mixer-ilb
  annotations:
    cloud.google.com/load-balancer-type: "internal"
  labels:
    istio: mixer
spec:
  type: LoadBalancer
  ports:
  - port: 8000
    protocol: TCP
  selector:
    istio: mixer
EOF

  PILOT_IP=$(kubectl get service istio-pilot-ilb -o jsonpath='{.status.loadBalancer.ingress[0].ip}')
  ISTIO_DNS=$(kubectl get -n kube-system service dns-ilb -o jsonpath='{.status.loadBalancer.ingress[0].ip}')
  MIXER_IP=$(kubectl get service mixer-ilb -o jsonpath='{.status.loadBalancer.ingress[0].ip}')

  #/etc/dnsmasq.d/kubedns
  echo "server=/default.svc.cluster.local/$ISTIO_DNS" > kubedns
  echo "address=/istio-mixer/$MIXER_IP" >> kubedns
  echo "address=/istio-pilot/$PILOT_IP" >> kubedns

  CIDR=$(gcloud container clusters describe ${K8SCLUSTER} --zone=${ISTIO_ZONE} --format "value(servicesIpv4Cidr)")
  echo "ISTIO_SERVICE_CIDR=$CIDR" > cluster.env

}

# Configure a service running on the VM.
# Params:
# - port of the service
# - service name (default to raw vm name)
# - IP of the rawvm (will get it from gcloud if not set)
#
function istioConfigHttpService() {
  local NAME=${1:-}
  local PORT=${2:-}
  local IP=${3:-}

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
      name: http

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
EOF


}


function istioRoute() {
cat <<EOF | kubectl create -f -
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
      - path: /index.html
        backend:
          serviceName: rawvm
          servicePort: 80
EOF

}


function istioProvisionTestWorker() {
 NAME=$1

 istioPrepareCluster

 istioRun $NAME "sudo rm -f istio-*.deb"

 # Copy deb, helper and config files
 istioCopy $NAME kubedns tools/deb/test/machine_setup.sh $PROXY_DIR/bazel-bin/tools/deb/*.deb


 istioRun $NAME "sudo ./machine_setup.sh"

 LOCAL_IP=$(istioVMInternalIP $NAME)

 istioConfigHttpService rawvm 80 $LOCAL_IP
}



function setUp() {

  istioVMInit ${TESTVM}

  istioProvisionTestWorker ${TESTVM}

}

function trearDown() {
  # TODO: it is also possible to reset the VM, may be faster
  istioVMDelete ${TESTVM}
}

setUp

echo Setup: $(istioVMExternalIP ${TESTVM})
