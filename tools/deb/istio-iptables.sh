#!/bin/bash
# Initialization script responsible for setting up port forwarding for Istio sidecar.

set -o nounset
set -o pipefail
IFS=,

ISTIO_SIDECAR_CONFIG=${ISTIO_SIDECAR_CONFIG:-/var/lib/istio/envoy/sidecar.env}
if [ ! -r $ISTIO_SIDECAR_CONFIG ]; then
  echo "Missing sidecar config: $ISTIO_SIDECAR_CONFIG"
  exit 1
fi

. $ISTIO_SIDECAR_CONFIG

if [ -z "${ENVOY_UID:-}" ]; then
  # Default to the UID of ENVOY_USER
  ENVOY_UID=$(id -u ${ENVOY_USER:-istio})
  if [ ! $? == 0 ]; then
     echo "Invalid istio user $ENVOY_UID $ENVOY_USER"
     exit 1
  fi
  ENVOY_UID=${ENVOY_UID},0
fi

if [ "${1:-}" = "clean" ]; then
  iptables -F -t nat
  iptables -t nat -X ISTIO_REDIRECT
  iptables -t nat -X ISTIO_OUTPUT
  exit 0
fi

# Remove the chains, to generate new configs.
iptables -t nat -X ISTIO_REDIRECT
iptables -t nat -X ISTIO_OUTPUT

# Create a new chain for redirecting inbound traffic to the common Envoy port.
iptables -t nat -N ISTIO_REDIRECT                                             -m comment --comment "istio/redirect-common-chain"
iptables -t nat -A ISTIO_REDIRECT -p tcp -j REDIRECT --to-port ${ENVOY_PORT:-15001}  -m comment --comment "istio/redirect-to-envoy-port"

# Makes sure SSH is not redirectred
iptables -t nat -A PREROUTING -p tcp --dport 22 -j RETURN

if [ "${ISTIO_LOCAL_PORTS:-}" == "*" ]; then
     for port in ${ISTIO_LOCAL_EXCLUDE_PORTS}; do
        iptables -t nat -A PREROUTING -p tcp --dport ${port} -j RETURN
     done
     iptables -t nat -A PREROUTING -p tcp -j ISTIO_REDIRECT
elif [ ! -z "${ISTIO_LOCAL_PORTS:-}" ]; then
    for port in ${ISTIO_LOCAL_PORTS}; do
        iptables -t nat -A PREROUTING -p tcp --dport ${port} -j ISTIO_REDIRECT
    done
fi

# Create a new chain for selectively redirecting outbound packets to Envoy.
iptables -t nat -N ISTIO_OUTPUT                                               -m comment --comment "istio/common-output-chain"

# Jump to the ISTIO_OUTPUT chain from OUTPUT chain for all tcp
# traffic. '-j RETURN' bypasses Envoy and '-j ISTIO_REDIRECT'
# redirects to Envoy.
iptables -t nat -A OUTPUT -p tcp -j ISTIO_OUTPUT                              -m comment --comment "istio/install-istio-output"

# Redirect app calls to back itself via Envoy when using the service VIP or endpoint
# address, e.g. appN => Envoy (client) => Envoy (server) => appN.
iptables -t nat -A ISTIO_OUTPUT -o lo ! -d 127.0.0.1/32 -j ISTIO_REDIRECT     -m comment --comment "istio/redirect-implicit-loopback"

for uid in ${ENVOY_UID}; do
  # Avoid infinite loops. Don't redirect Envoy traffic directly back to
  # Envoy for non-loopback traffic.
  iptables -t nat -A ISTIO_OUTPUT -m owner --uid-owner ${uid} -j RETURN   -m comment --comment "istio/bypass-envoy"
done

# Skip redirection for Envoy-aware applications and
# container-to-container traffic both of which explicitly use
# localhost.
iptables -t nat -A ISTIO_OUTPUT -d 127.0.0.1/32 -j RETURN                     -m comment --comment "istio/bypass-explicit-loopback"

IFS=,
if [ "${ISTIO_SERVICE_CIDR}" != "" ]; then
    for cidr in ${ISTIO_SERVICE_CIDR}; do
        iptables -t nat -A ISTIO_OUTPUT -d ${cidr} -j ISTIO_REDIRECT          -m comment --comment "istio/redirect-ip-range-${cidr}"
    done
    iptables -t nat -A ISTIO_OUTPUT -j RETURN                                 -m comment --comment "istio/bypass-default-outbound"
else
    iptables -t nat -A ISTIO_OUTPUT -j ISTIO_REDIRECT                         -m comment --comment "istio/redirect-default-outbound"
fi

