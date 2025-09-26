#!/bin/sh
set -e

if otool -L $1 | grep -q libtest; then
    >&2 echo "error: rust_binary is dynamically linked against libtest!"
    exit 1
fi

if otool -L $1 | grep -q libstd; then
    >&2 echo "error: rust_binary is dynamically linked against libstd!"
    exit 1
fi
