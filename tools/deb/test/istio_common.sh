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

# Git sync
function istio_sync() {
  # TODO: use repo sync instead
  local BRANCH=${1-master}
  mkdir -p $GOPATH/src/istio.io

  for sub in pilot istio mixer auth proxy; do
    if [[ -d $GOPATH/src/istio.io/$sub ]]; then
      (cd $GOPATH/src/istio.io/$sub; git pull origin $BRANCH)
    else
      (cd $GOPATH/src/istio.io; git clone https://github.com/istio/$sub; )
    fi
  done

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

# Helper to get the external IP of a raw VM
function istioVMExternalIP() {
  local NAME=${1:-$TESTVM}
  gcloud compute --project $PROJECT instances describe $NAME --zone $ISTIO_ZONE --format='value(networkInterfaces[0].accessConfigs[0].natIP)'
}

function istioVMInternalIP() {
  local NAME=${1:-$TESTVM}
  gcloud compute --project $PROJECT instances describe $NAME  --zone $ISTIO_ZONE --format='value(networkInterfaces[0].networkIP)'
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

# Helper to use the raw istio config templates directly.
function istioTmpl() {
  local FILE=$1

  local SED="s,{PROXY_HUB},$HUB,; s,{PROXY_TAG},$TAG,; "
  SED="$SED s,{MIXER_HUB},$HUB,; s,{MIXER_TAG},$TAG,; "
  SED="$SED s,{PILOT_HUB},$HUB,; s,{PILOT_TAG},$TAG,; "
  SED="$SED s,{CA_HUB},$HUB,; s,{CA_TAG},$TAG,; "

  sed  "$SED" $1

}
# Install Istio components. The test environment may be already setup, this function is needed to quickly setup
# the components for the rawVM testing in a new cluster.
function istioInstallCluster() {
    pushd $GOPATH/src/istio.io/istio/install/kubernetes/templates
    istioTmpl istio-ingress.yaml | kubectl apply -f -
    istioTmpl istio-mixer.yaml | kubectl apply -f -
    istioTmpl istio-pilot.yaml | kubectl apply -f -
    cd istio-auth
    istioTmpl istio-namespace-ca.yaml | kubectl apply -f -

    cd ../../addons
    kubectl apply -f grafana.yaml
    kubectl apply -f prometheus.yaml
    kubectl apply -f zipkin.yaml

    ISTIOCTL=$GOPATH/src/istio.io/pilot/bazel-bin/cmd/istioctl/istioctl

    cd ../../../samples/apps/bookinfo
    kubectl apply -f <($ISTIOCTL kube-inject --hub $HUB --tag $TAG -f bookinfo.yaml)

    popd
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
  - port: 9091
    protocol: TCP
  selector:
    istio: mixer
EOF

cat <<EOF | kubectl apply -f -
apiVersion: v1
kind: Service
metadata:
  name: istio-ca-ilb
  annotations:
    cloud.google.com/load-balancer-type: "internal"
  labels:
    istio: istio-ca
spec:
  type: LoadBalancer
  ports:
  - port: 8060
    protocol: TCP
  selector:
    istio: istio-ca
EOF

  for i in {1..10}
  do
    PILOT_IP=$(kubectl get service istio-pilot-ilb -o jsonpath='{.status.loadBalancer.ingress[0].ip}')
    ISTIO_DNS=$(kubectl get -n kube-system service dns-ilb -o jsonpath='{.status.loadBalancer.ingress[0].ip}')
    MIXER_IP=$(kubectl get service mixer-ilb -o jsonpath='{.status.loadBalancer.ingress[0].ip}')
    CA_IP=$(kubectl get service istio-ca-ilb -o jsonpath='{.status.loadBalancer.ingress[0].ip}')

    if [ ${PILOT_IP} == "" -o  ${PILOT_IP} == "" -o ${MIXER_IP} == "" ] ; then
        echo Waiting for ILBs
        sleep 10
    else
        break
    fi
  done

  if [ ${PILOT_IP} == "" -o  ${PILOT_IP} == "" -o ${MIXER_IP} == "" ] ; then
    echo "Failed to create ILBs"
    exit 1
  fi

  #/etc/dnsmasq.d/kubedns
  echo "server=/default.svc.cluster.local/$ISTIO_DNS" > kubedns
  echo "address=/istio-mixer/$MIXER_IP" >> kubedns
  echo "address=/mixer-server/$MIXER_IP" >> kubedns
  echo "address=/istio-pilot/$PILOT_IP" >> kubedns
  echo "address=/istio-ca/$CA_IP" >> kubedns

  CIDR=$(gcloud container clusters describe ${K8SCLUSTER} --project ${PROJECT} --zone=${ISTIO_ZONE} --format "value(servicesIpv4Cidr)")
  echo "ISTIO_SERVICE_CIDR=$CIDR" > cluster.env

}

function istioSSHVM() {
  local NAME=${1:-testvm}

  gcloud compute ssh --project $PROJECT --zone $ISTIO_ZONE $NAME
}
