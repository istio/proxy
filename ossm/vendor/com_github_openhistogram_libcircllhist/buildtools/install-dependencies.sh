#!/bin/bash

CIRC_PYTHON=/opt/circonus/python27/bin/python

if [ -z "$PYTHON_BIN" ]
then
    if [ -x "$CIRC_PYTHON" ]
    then
        PYTHON_BIN=$CIRC_PYTHON
    else
        PYTHON_BIN="$(command -v python)"
    fi
fi

if [ ! -x "$PYTHON_BIN" ]
then
    echo "Python executable not found: $PYTHON_BIN"
    exit 1
fi

pushd `dirname $0`/../src/python > /dev/null

if ! $PYTHON_BIN -c "import cffi" > /dev/null 2>&1;
then
    echo "Using Python interpreter $PYTHON_BIN"
    $PYTHON_BIN --version
    echo "Installing Python CFFI"
    if ! $PYTHON_BIN setup.py develop --user > /dev/null 2>&1;
    then
        $PYTHON_BIN -m pip install cffi > /dev/null
    fi
fi

popd > /dev/null
