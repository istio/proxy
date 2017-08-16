#!/bin/bash

# Script to run Envoy for Istio.

# Load config variables
set -e

ISTIO_SIDECAR_CONFIG=${ISTIO_SIDECAR_CONFIG:-/var/lib/istio/envoy/sidecar.env}

if [[ -r ${ISTIO_SIDECAR_CONFIG} ]]; then
  . $ISTIO_SIDECAR_CONFIG
fi

ISTIO_BIN_BASE=${ISTIO_BIN_BASE:-/usr/local/bin}
ISTIO_LOG_DIR=${ISTIO_LOG_DIR:-/var/log/istio}
ISTIO_CFG=${ISTIO_CFG:-/var/lib/istio}

# Update iptables
${ISTIO_BIN_BASE}/istio-iptables.sh


if [ -f ${ISTIO_BIN_BASE}/pilot-agent ]; then
  exec su - istio ${ISTIO_BIN_BASE}/pilot-agent proxy > ${ISTIO_LOG_DIR}/istio.log 2>&1
else
  if [ -z "${ISTIO_SVC_IP:-}" ]; then
    ISTIO_SVC_IP=$(hostname --ip-address)
  fi
  # Run envoy directly - agent not installed. This should be used only for debugging/testing standalone envoy
  exec su - istio -c "${ISTIO_BIN_BASE}/envoy -c ${ISTIO_CFG}/envoy/envoy.json --restart-epoch 0 --drain-time-s 2 --parent-shutdown-time-s 3 --service-cluster istio-proxy --service-node 'sidecar~${ISTIO_SVC_IP}~mysvc.${ISTIO_NAMESPACE:-default}~cluster.local' $ISTIO_DEBUG"
fi


