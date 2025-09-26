#!/usr/bin/env bash

set -e -o pipefail

su - envoybuild -c "git config --global --add safe.directory /workspace/envoy"
mkdir -p /workspace/envoy/generated/docs
chown envoybuild:envoybuild /workspace/envoy/generated/docs
