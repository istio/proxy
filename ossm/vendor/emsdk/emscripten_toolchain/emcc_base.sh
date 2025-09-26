#!/bin/bash

source $(dirname $0)/env.sh

PYTHON3="${PYTHON3:-python3}"

exec $PYTHON3 $EMSCRIPTEN/emcc.py "$@"
