#!/usr/bin/env bash

exec /usr/local/bin/pilot-agent proxy > /var/lib/istio/istio.log 2>&1
