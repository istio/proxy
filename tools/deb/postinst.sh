#!/bin/bash

set -e

action="$1"
oldversion="$2"

umask 022

if ! getent passwd istio-proxy >/dev/null; then
    addgroup --system istio-proxy
    adduser --system --group --home /var/lib/istio istio-proxy
fi

if [ ! -e /etc/istio ]; then
   # Backward compat.
   ln -s /var/lib/istio /etc/istio
fi

mkdir -p /var/lib/istio/envoy
mkdir -p /var/lib/istio/proxy
mkdir -p /var/lib/istio/config
mkdir -p /var/log/istio

touch /var/lib/istio/config/mesh

chown istio-proxy.istio-proxy /var/lib/istio/envoy /var/lib/istio/config /var/log/istio /var/lib/istio/config/mesh /var/lib/istio/proxy

