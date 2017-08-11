#!/bin/bash
# Initialization script responsible for setting up port forwarding for Istio sidecar.

set -o errexit
set -o nounset
set -o pipefail

usage() {
  echo "${0} -p ENVOY_PORT -u UID -s IN_SERVICE_PORTS [-h]"
  echo ''
  echo '  -p: Specify the envoy port to which redirect all TCP traffic'
  echo '  -u: Comma separate list of UIDs for which the redirection is not'
  echo '      applied. Typically, this is the UID of the proxy container'
  echo '  -i: Comma separated list of IP ranges in CIDR form to redirect to envoy (optional)'
  echo '  -s: Comma separated list of incoming service IPs to intercept (optional)'
  echo ''
}

IP_RANGES_INCLUDE=""

while getopts ":p:u:e:i:h:s" opt; do
  case ${opt} in
    p)
      ENVOY_PORT=${OPTARG}
      ;;
    u)
      ENVOY_UID=${OPTARG}
      ;;
    i)
      IP_RANGES_INCLUDE=${OPTARG}
      ;;
    s)
      ISTIO_LOCAL_PORTS=${OPTARG}
      ;;
    h)
      usage
      exit 0
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      usage
      exit 1
      ;;
  esac
done

if [[ -z "${ENVOY_PORT-}" ]]; then
  ENVOY_PORT=15001
fi

if [[ -z "${ENVOY_UID-}" ]]; then
  ENVOY_UID=`id -u ${ENVOY_USER-istio}`
  ENVOY_UID=${ENVOY_UID},0
fi

if [ "$1" = "clean" ]; then
  iptables -F -t nat
  iptables -t nat -X ISTIO_REDIRECT
  iptables -t nat -X ISTIO_OUTPUT
  exit 0
fi


# Create a new chain for redirecting inbound traffic to the common Envoy port.
iptables -t nat -X ISTIO_REDIRECT
iptables -t nat -N ISTIO_REDIRECT                                             -m comment --comment "istio/redirect-common-chain"
iptables -t nat -A ISTIO_REDIRECT -p tcp -j REDIRECT --to-port ${ENVOY_PORT}  -m comment --comment "istio/redirect-to-envoy-port"

IFS=,
if [ "${ISTIO_LOCAL_PORTS}" == "*" ]; then
    # Redirect all inbound traffic to Envoy.
    iptables -t nat -A PREROUTING -j ISTIO_REDIRECT                               -m comment --comment "istio/install-istio-prerouting"
else
    for port in ${SERVICE_PORTS}; do
        iptables -t nat -A PREROUTING -t tcp --dport ${port} -j ISTIO_REDIRECT
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
if [ "${IP_RANGES_INCLUDE}" != "" ]; then
    for cidr in ${IP_RANGES_INCLUDE}; do
        iptables -t nat -A ISTIO_OUTPUT -d ${cidr} -j ISTIO_REDIRECT          -m comment --comment "istio/redirect-ip-range-${cidr}"
    done
    iptables -t nat -A ISTIO_OUTPUT -j RETURN                                 -m comment --comment "istio/bypass-default-outbound"
else
    iptables -t nat -A ISTIO_OUTPUT -j ISTIO_REDIRECT                         -m comment --comment "istio/redirect-default-outbound"
fi

