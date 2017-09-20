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

# Environment variables used by functions:
# - VMNAME - name of the VM
# - K8SCLUSTER - name of the K8S cluster
# - PROJECT - project


# Get an istio service account secret, extract it to files to be provisioned on a raw VM
function istio_provision_certs() {
  local SA=${1:-istio.default}

  kubectl get secret $SA -o jsonpath='{.data.cert-chain\.pem}' |base64 -d  > cert-chain.pem
  kubectl get secret $SA -o jsonpath='{.data.root-cert\.pem}' |base64 -d  > root-cert.pem
  kubectl get secret $SA -o jsonpath='{.data.key\.pem}' |base64 -d  > key.pem

}

# Install required files on a VM and run the setup script.
function istioProvisionVM() {
 local NAME=${1:-$VMNAME}

 local SA=${2:-istio.default}
 local ISTIO_IO=${ISTIO_BASE:-${GOPATH:-$HOME/go}}/src/istio.io
 pushd $ISTIO_IO/proxy
 istio_provision_certs $SA

 # Remove any old files
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
   $ISTIO_IO/auth/bazel-bin/tools/deb/istio-auth-node-agent.deb \
   $ISTIO_IO/istio/samples/apps/bookinfo/src/details \
   $ISTIO_IO/istio/samples/apps/bookinfo/src/mysql \
   $ISTIO_IO/istio/samples/apps/bookinfo/src/productpage \
   $ISTIO_IO/istio/samples/apps/bookinfo/src/ratings


 istioRun $NAME "sudo bash -c -x ./machine_setup.sh $NAME"
 popd
}


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
  local ISTIOCTL=$GOPATH/src/istio.io/pilot/bazel-bin/cmd/istioctl/istioctl

  $ISTIOCTL register $NAME $IP $PORT:$TYPE
}


# Helper to get the external IP of a raw VM
function istioVMExternalIP() {
  local NAME=${1:-$VMNAME}
  gcloud compute instances describe $NAME  $(_istioGcloudOpt) --format='value(networkInterfaces[0].accessConfigs[0].natIP)'
}

function istioVMInternalIP() {
  local NAME=${1:-$VMNAME}
  gcloud compute instances describe $NAME  $(_istioGcloudOpt) --format='value(networkInterfaces[0].networkIP)'
}



# Helpers for build / install

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

# Show the branch and status of each istio repo
function istio_status() {
  cd $GOPATH/src/istio.io

  for sub in pilot istio mixer auth proxy; do
     echo -e "\n\n$sub\n"
     (cd $GOPATH/src/istio.io/$sub; git branch; git status)
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


# Copy files to the VM
function istioVMCopy() {
  # TODO: based on some env variable, use different commands for other clusters or for testing with
  # bare-metal machines.
  local NAME=$1
  shift
  local FILES=$*
  local ISTIO_ZONE=${ISTIO_ZONE:-us-west1-c}

  local OPTS=""
  if [[ ${ISTIO_ZONE:-} != "" ]]; then
     OPTS="$OPTS --zone $ISTIO_ZONE"
  fi
  gcloud compute scp  $OPTS --recurse $FILES ${NAME}:
}



function ingressIP() {
  kubectl get service istio-ingress -o jsonpath='{.status.loadBalancer.ingress[0].ip}'
}

# Copy files to the VM
function istioCopy() {
  # TODO: based on some env variable, use different commands for other clusters or for testing with
  # bare-metal machines.
  local NAME=$1
  shift
  local FILES=$*

  gcloud compute scp --recurse $(_istioGcloudOpt) $FILES ${NAME}:
}


# Create the raw VM.
function istioVMInit() {
  # TODO: check if it exists and do "reset", to speed up the script.
  local NAME=$1
  local IMAGE=${2:-debian-9-stretch-v20170816}
  local IMAGE_PROJECT=${3:-debian-cloud}

  gcloud compute instances  describe $NAME  $(_istioGcloudOpt) >/dev/null 2>/dev/null
  if [[ $? == 0 ]] ; then
    gcloud compute instances delete $NAME $(_istioGcloudOpt)
  fi

  gcloud compute \
     instances create $NAME \
     $(_istioGcloudOpt) \
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
  local OPTS=""
  if [[ ${PROJECT:-} != "" ]]; then
       OPTS="--project $PROJECT"
  fi

  gcloud compute $OPTS firewall-rules create allow-external  --allow tcp:22,tcp:80,tcp:443,tcp:9411,udp:5228,icmp  --source-ranges 0.0.0.0/0

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
  local NAME=${1:-$VMNAME}
  gcloud compute -q  instances delete $NAME $(_istioGcloudOpt)
}



# Run a command in a VM.
function istioRun() {
  local NAME=$1
  local CMD=$2

  gcloud compute ssh $(_istioGcloudOpt) $NAME --command "$CMD"
}


# Helper to use the raw istio config templates directly.
function istioTmpl() {
  local FILE=$1
  local TAG=${TAG:-$(whoami)}
  local HUB=${HUB:-gcr.io/istio-testing}

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
    local OPTS=""
    if [[ ${PROJECT:-} != "" ]]; then
       OPTS="--project $PROJECT"
    fi
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

  CIDR=$(gcloud container clusters describe ${K8SCLUSTER} $(_istioGcloudOpt) --format "value(servicesIpv4Cidr)")
  echo "ISTIO_SERVICE_CIDR=$CIDR" > cluster.env

}

# PROJECT and ISTIO_ZONE are optional. If not set, the defaults will be used.
# Example default setting:
# gcloud config set compute/zone us-central1-a
# gcloud config set core/project costin-raw
function _istioGcloudOpt() {
    local OPTS=""
    if [[ ${PROJECT:-} != "" ]]; then
       OPTS="--project $PROJECT"
    fi
    if [[ ${ISTIO_ZONE:-} != "" ]]; then
       OPTS="$OPTS --zone $ISTIO_ZONE"
    fi
    echo $OPTS
}

function istioSSHVM() {
  local NAME=${1:-${VMNAME}}

  gcloud compute ssh $(_istioGcloudOpt) $NAME
}

# Configure a service running on the VM.
# Params:
# - port of the service
# - service name (default to raw vm name)
# - IP of the rawvm (will get it from gcloud if not set)
# - type - defaults to http
#
function istioConfigServiceKubeCtl() {
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



