#!/usr/bin/env bash

set -o pipefail -o errexit -o nounset

exec "$JS_BINARY__NODE_BINARY" --require "$JS_BINARY__NODE_PATCHES" "$@"
