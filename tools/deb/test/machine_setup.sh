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

NAME=${1-$(hostname)}

# Script to run on a machine to init DNS and other packages.
# Used for automated testing of raw VM setup

# Packages required for istio DNS
PACKAGES="dnsutils dnsmasq"

# Debugging
PACKAGES="$PACKAGES tcpdump netcat tmux"

# Used by tests
PACKAGES="$PACKAGES nginx ruby python-pip mariadb-server "

# Install nodejs. Version in debian is old
curl -sL https://deb.nodesource.com/setup_8.x | sudo -E bash -
sudo apt-get install -y nodejs

apt-get update
sudo apt-get -y install $PACKAGES

# Copy config files for DNS
chmod go+r kubedns
cp kubedns /etc/dnsmasq.d
systemctl restart dnsmasq

# Cluster settings - the CIDR in particular.
cp cluster.env /var/lib/istio/envoy

echo "ISTIO_INBOUND_PORTS=80" > /var/lib/istio/envoy/sidecar.env

# Update DHCP - if needed
grep "^prepend domain-name-servers 127.0.0.1;" /etc/dhcp/dhclient.conf > /dev/null
if [[ $? != 0 ]]; then
  echo 'prepend domain-name-servers 127.0.0.1;' >> /etc/dhcp/dhclient.conf
  # TODO: find a better way to re-trigger dhclient
  dhclient -v -1
fi

# Install istio binaries
dpkg -i istio-proxy-envoy_0.2.1_amd64.deb;
dpkg -i istio-agent_0.2.1_amd64.deb;
# TODO: add auth agent when ready

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

(cd productpage; sudo pip install -r requirements.txt)
(cd ratings; npm install)

systemctl restart nginx

# Start istio
systemctl start istio

systemctl start mariadb

# Start bookinfo components
ruby details/details.rb 9080 &
echo $! > details.pid

# Note that we run productpage on a different port - 9080 is taken
(cd productpage ; python productpage.py 9081 &)
echo $! > productpage.pid

(cd ratings ; node ratings.js 9082 &)
echo $! > ratings.pid
