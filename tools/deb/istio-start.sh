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
  exec su -s /bin/bash -c "exec ${ISTIO_BIN_BASE}/pilot-agent proxy > ${ISTIO_LOG_DIR}/istio.log 2>&1" istio-proxy
else
  if [ -z "${ISTIO_SVC_IP:-}" ]; then
    ISTIO_SVC_IP=$(hostname --ip-address)
  fi
  ENVOY_CFG=${ENVOY_CFG:-${ISTIO_CFG}/envoy/envoy.json}
  # Run envoy directly - agent not installed. This should be used only for debugging/testing standalone envoy
  exec su -s /bin/bash -c "exec ${ISTIO_BIN_BASE}/envoy -c $ENVOY_CFG --restart-epoch 0 --drain-time-s 2 --parent-shutdown-time-s 3 --service-cluster istio-proxy --service-node 'sidecar~${ISTIO_SVC_IP}~mysvc.${ISTIO_NAMESPACE:-default}~cluster.local' $ISTIO_DEBUG" istio-proxy
fi

