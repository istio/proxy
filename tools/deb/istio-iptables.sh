#!/bin/bash
#
# Copyright 2016 Istio Authors. All Rights Reserved.
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
#
# Initialization script responsible for setting up port forwarding for Istio sidecar.

# Based on pilot/docker/prepare_proxy.sh - but instead of capturing all traffic, only capture
# configured ranges.
# Compared to the K8S docker sidecar:
# - use config files - manual or pushed by an config system.
# - fine grain control over what inbound ports are captured
# - more control over what outbound traffic is captured
# - can be run multiple times, will cleanup previous rules
# - the "clean" option will remove all rules it previously added.

# After more testing, the goal is to replace and unify the script in K8S - by generating
# the sidecar image using the .deb file created by proxy.

set -o nounset
set -o pipefail
IFS=,

ISTIO_SIDECAR_CONFIG=${ISTIO_SIDECAR_CONFIG:-/var/lib/istio/envoy/sidecar.env}
if [ -r ${ISTIO_SIDECAR_CONFIG} ]; then
  . ${ISTIO_SIDECAR_CONFIG}
fi

# TODO: more flexibility - maybe a whitelist of users to be captured for output instead of
# a blacklist.
if [ -z "${ENVOY_UID:-}" ]; then
  # Default to the UID of ENVOY_USER and root
  ENVOY_UID=$(id -u ${ENVOY_USER:-istio-proxy})
  if [ ! $? == 0 ]; then
     echo "Invalid istio user $ENVOY_UID $ENVOY_USER"
     exit 1
  fi
  ENVOY_UID=${ENVOY_UID},0
fi

# Remove the old chains, to generate new configs.
iptables -t nat -D PREROUTING -p tcp -j ISTIO_INBOUND 2>/dev/null
iptables -t nat -D OUTPUT -p tcp -j ISTIO_OUTPUT 2>/dev/null

# Flush and delete the istio chains
iptables -t nat -F ISTIO_OUTPUT 2>/dev/null
iptables -t nat -X ISTIO_OUTPUT 2>/dev/null
iptables -t nat -F ISTIO_INBOUND 2>/dev/null
iptables -t nat -X ISTIO_INBOUND 2>/dev/null
iptables -t nat -F ISTIO_REDIRECT 2>/dev/null
iptables -t nat -X ISTIO_REDIRECT 2>/dev/null

if [ "${1:-}" = "clean" ]; then
  # Only cleanup, don't add new rules.
  exit 0
fi

# Create a new chain for redirecting inbound traffic to the common Envoy port.
# In the ISTIO_INBOUND and ISTIO_OUTBOUND chains, '-j RETURN' bypasses Envoy
# and '-j ISTIO_REDIRECT' redirects to Envoy.
iptables -t nat -N ISTIO_REDIRECT
iptables -t nat -A ISTIO_REDIRECT -p tcp -j REDIRECT --to-port ${ENVOY_PORT:-15001}

# Handling of inbound ports. Traffic will be redirected to Envoy, which will process and forward
# to the local service.
if [ -n "${ISTIO_INBOUND_PORTS:-}" ]; then
  iptables -t nat -N ISTIO_INBOUND
  iptables -t nat -A PREROUTING -p tcp -j ISTIO_INBOUND

  # Makes sure SSH is not redirectred
  iptables -t nat -A ISTIO_INBOUND -p tcp --dport 22 -j RETURN

  if [ "${ISTIO_INBOUND_PORTS:-}" == "*" ]; then
       for port in ${ISTIO_LOCAL_EXCLUDE_PORTS:-}; do
          iptables -t nat -A ISTIO_INBOUND -p tcp --dport ${port} -j RETURN
       done
       iptables -t nat -A ISTIO_INBOUND -p tcp -j ISTIO_REDIRECT
  else
      for port in ${ISTIO_INBOUND_PORTS}; do
          iptables -t nat -A ISTIO_INBOUND -p tcp --dport ${port} -j ISTIO_REDIRECT
      done
  fi
fi

# Create a new chain for selectively redirecting outbound packets to Envoy.
iptables -t nat -N ISTIO_OUTPUT

# Jump to the ISTIO_OUTPUT chain from OUTPUT chain for all tcp traffic.
iptables -t nat -A OUTPUT -p tcp -j ISTIO_OUTPUT

# Redirect app calls to back itself via Envoy when using the service VIP or endpoint
# address, e.g. appN => Envoy (client) => Envoy (server) => appN.
iptables -t nat -A ISTIO_OUTPUT -o lo ! -d 127.0.0.1/32 -j ISTIO_REDIRECT

for uid in ${ENVOY_UID}; do
  # Avoid infinite loops. Don't redirect Envoy traffic directly back to
  # Envoy for non-loopback traffic.
  iptables -t nat -A ISTIO_OUTPUT -m owner --uid-owner ${uid} -j RETURN
done

# Skip redirection for Envoy-aware applications and
# container-to-container traffic both of which explicitly use
# localhost.
iptables -t nat -A ISTIO_OUTPUT -d 127.0.0.1/32 -j RETURN

IFS=,
if [ -n "${ISTIO_SERVICE_CIDR:-}" ]; then
    for cidr in ${ISTIO_SERVICE_CIDR}; do
        iptables -t nat -A ISTIO_OUTPUT -d ${cidr} -j ISTIO_REDIRECT
    done
    iptables -t nat -A ISTIO_OUTPUT -j RETURN
else
    iptables -t nat -A ISTIO_OUTPUT -j ISTIO_REDIRECT
fi

