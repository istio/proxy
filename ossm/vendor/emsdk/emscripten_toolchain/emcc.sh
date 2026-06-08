#!/bin/bash -e

source $(dirname $0)/env.sh

if [[ "$EMSDK_PYTHON" != /* ]]; then
    EMSDK_PYTHON="$ROOT_DIR/$EMSDK_PYTHON"
fi

PYBINPATH="$(dirname "${EMSDK_PYTHON}")"
export PATH=$PYBINPATH:$PATH

exec $EMSDK_PYTHON $EMSCRIPTEN/emcc.py "$@"
