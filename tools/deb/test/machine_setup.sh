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
set -x

# Script run on the test VM, to install istio components and test fixtures.
# The machine needs to be re-imaged to test 'clean' installs, or reused to test
# 'upgrade' use cases - the script should work for both cases.


# Configure DHCP server to use DNSMasq, configure dnsmasq.
function istioNetworkInit() {
  apt-get update
  # Packages required for istio DNS
  sudo apt-get -y install dnsutils dnsmasq

  # Cluster settings - the CIDR in particular.
  cp cluster.env /var/lib/istio/envoy

  # Copy config files for DNS
  chmod go+r kubedns
  cp kubedns /etc/dnsmasq.d
  systemctl restart dnsmasq

  # Update DHCP - if needed
  grep "^prepend domain-name-servers 127.0.0.1;" /etc/dhcp/dhclient.conf > /dev/null
  if [[ $? != 0 ]]; then
    echo 'prepend domain-name-servers 127.0.0.1;' >> /etc/dhcp/dhclient.conf
    # TODO: find a better way to re-trigger dhclient
    dhclient -v -1
  fi

  chown -R istio-proxy /var/lib/istio/envoy
}

# Install istio components and certificates. The admin (directly or using tools like ansible)
# will generate and copy the files and install the packages on each machine.
function istioInstall() {
  mkdir -p /etc/certs
  cp *.pem /etc/certs
  chown -R istio-proxy /etc/certs
  # Install istio binaries
  dpkg -i istio-proxy-envoy.deb
  dpkg -i istio-agent.deb
  dpkg -i istio-node-agent.deb

  echo "ISTIO_INBOUND_PORTS=80,9080,3306,27017" > /var/lib/istio/envoy/sidecar.env
  chown istio-proxy /var/lib/istio/envoy/sidecar.env
}

function istioRestart() {
    # Start or restart istio
    systemctl status istio > /dev/null
    if [[ $? = 0 ]]; then
      systemctl restart istio
    else
      systemctl start istio
    fi
}


## Test helpers

# Install a web server and helper packages to help testing and debugging
# We use a web server to verify HTTP requests can be made from K8S to raw VM and from
# raw VM to K8S services (zipkin in this test)
function istioInstallTestHelpers() {
  NAME=${1-$(hostname)}

  sudo apt-get -y install nginx tcpdump netcat tmux

  mkdir /var/www/html/$NAME
  echo "VM $NAME" > /var/www/html/$NAME/index.html

  cat <<EOF > /etc/nginx/conf.d/zipkin.conf
  server {
        listen 9411;
        location / {
          proxy_pass http://zipkin.default.svc.cluster.local:9411/;
          proxy_http_version 1.1;
        }
      }
EOF


  systemctl restart nginx
}

function istioMysql() {
  # The ratings script is using root/password by default.

  cat <<EOF | sudo mysql
GRANT ALL PRIVILEGES on *.* to 'root'@'localhost' IDENTIFIED BY 'password';
FLUSH PRIVILEGES;
CREATE DATABASE test;
EOF

  # Create the tables
  mysql -u root -ppassword < mysql/mysqldb-init.sql
}

function istioUpdateRatings() {
  local RATING=${1-2}

  cat <<EOF | mysql -u root -ppassword
  USE test;
  update ratings set rating=$RATING where reviewid=1;
EOF
}

function istioInstallBookinfo() {
  # Install nodejs. Version in debian is old
  curl -sL https://deb.nodesource.com/setup_8.x | sudo -E bash -
  sudo apt-get install -y nodejs ruby  python-pip mariadb-server mongodb
  (cd productpage; sudo pip install -r requirements.txt)
  (cd ratings; npm install)

  systemctl start mariadb
  istioMysql

  # Start bookinfo components
  if [[ -r details.pid ]] ; then
     kill -9 $(cat details.pid)
  fi
  ruby details/details.rb 9080  > details.log 2>&1 &
  echo $! > details.pid

  # Note that we run productpage on a different port - 9080 is taken
  #(cd productpage ; python productpage.py 9081 &)
  #echo $! > productpage.pid

  #(cd ratings ; node ratings.js 9082 &)
  #echo $! > ratings.pid
}

echo "Initializing VM ${@}"

if [[ ${1:-} == "initNetwork" ]] ; then
  istioNetworkInit
elif [[ ${1:-} == "istioInstall" ]] ; then
  istioInstall
  istioRestart
elif [[ ${1:-} == "istioInstallTest" ]] ; then
  istioInstallTestHelpers
  istioInstallBookinfo
elif [[ ${1:-} == "env" ]] ; then
  echo "Istio functions loaded"
elif [[ ${1:-} == "help" ]] ; then
  echo "$0 init: provision an existing VM using the current build"
  echo "$0 env: load the istio functions"
  echo "$0 : run all steps"
else
    istioNetworkInit

    istioInstall

    istioInstallTestHelpers

    istioInstallBookinfo

    istioRestart
    exit 0
fi


