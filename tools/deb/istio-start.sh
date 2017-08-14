#!/bin/bash

# Script to run Envoy for Istio.

# Load config variables
set -e

ISTIO_SIDECAR_CONFIG=${ISTIO_SIDECAR_CONFIG:-/var/lib/istio/envoy/sidecar.env}

if [[! -f $ISTIO_SIDECAR_CONFIG]]; then
  echo "Missing sidecar config: $ISTIO_SIDECAR_CONFIG"
  exit 1
fi

. $ISTIO_SIDECAR_CONFIG

/usr/local/bin/istio-iptables.sh


if [ -f /usr/local/bin/pilot-agent ]; then
  exec su - istio /usr/local/bin/pilot-agent proxy > /var/log/istio/istio.log 2>&1
else
  if [ "" = "$SVC_IP" ]; then
    SVC_IP=$(hostname --ip-address)
  fi
  # Run envoy directly - agent not installed. This should be used only for debugging/testing standalone envoy
  exec su - istio -c "/usr/local/bin/envoy -c /var/lib/istio/envoy/envoy.json --restart-epoch 0 --drain-time-s 2 --parent-shutdown-time-s 3 --service-cluster istio-proxy --service-node '$SVC_IP|mysvc.default|cluster.local' $ISTIO_DEBUG"
fi


