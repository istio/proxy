#!/bin/bash

# Dummy dwp script for emscripten toolchain
# Since wasm doesn't support split debug info, this is a no-op
# Just echo the command for debugging and exit successfully

echo "DWP called with args: $@" >&2
exit 0
