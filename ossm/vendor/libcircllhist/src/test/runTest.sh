#!/bin/bash
set -e

pushd `dirname $0`
export LD_LIBRARY_PATH=..:/opt/circonus/lib/amd64:/opt/circonus/lib
export DYLD_LIBRARY_PATH=..:/opt/circonus/lib/amd64:/opt/circonus/lib


echo "Running C tests"
./histogram_test


echo "Running LuaJIT tests"
CIRC_LUA="/opt/circonus/bin/luajit"
if [ -z "$LUA_BIN" ]
then
    set +e
    if [ -x "$CIRC_LUA" ]
    then
        LUA_BIN=$CIRC_LUA
    else
        LUA_BIN="$(command -v luajit)"
    fi
    set -e
fi

if [ -x "$LUA_BIN" ]
then
    echo "Using Lua interpreter $LUA_BIN"
    $LUA_BIN -v
else
    echo "Lua executable not found: $LUA_BIN"
    exit 1
fi

export LUA_PATH="\
/opt/circonus/share/lua/5.1/?.lua;\
?.lua;\
../lua/?.lua;\
$($LUA_BIN -e "print(package.path)")"

export LUA_CPATH="\
/opt/circonus/share/lua/5.1/?.so;\
/opt/circonus/lib/amd64/lua/5.1/?.so;\
/opt/circonus/lib/lua/5.1/?.so;\
"

echo "set LUA_PATH=$LUA_PATH"

export LUA_INIT=''
arg=''
DEBUG_PREFIX=""

echo "Lua histogram_c_test"
$LUA_BIN $LUA_ARGS -l histogram_c_test -e "histogram_c_test.runTests()"

echo "Lua circllhist_test"
$LUA_BIN $LUA_ARGS -l circllhist_test -e "circllhist_test.runTests()"


echo "Running Python tests"
pushd ../python

if [ -z "$PYTHON_BIN" ]
then
    set +e
    PYTHON_BIN="$(command -v python)"

    if [ ! -x "$PYTHON_BIN" ]
    then
        PYTHON_BIN="$(command -v python3)"
    fi
    set -e
fi

if [ -x "$PYTHON_BIN" ]
then
    echo "Using Python interpreter $PYTHON_BIN"
    $PYTHON_BIN --version
else
    echo "Python executable not found: $PYTHON_BIN"
    exit 1
fi

echo "Running: $PYTHON_BIN test.py"
$PYTHON_BIN test.py

popd

popd
