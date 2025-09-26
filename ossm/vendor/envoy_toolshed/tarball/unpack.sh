#!/bin/bash -e

set -o pipefail

ZSTD="${ZSTD:-}"
TARGET="${TARGET:-}"
OVERWRITE="${OVERWRITE:-}"
EXTRACT_PATH="$1"

if [[ -z "$TARGET" || "$TARGET" == "PLACEHOLDER.TXT" ]]; then
    echo "TARGET must be provided using a bazel flag." >&2
    exit 1
fi

if [[ -z "$EXTRACT_PATH" ]]; then
    echo "EXTRACT_PATH must be provided as an arg." >&2
    exit 1
fi

if [[ ! -e "$EXTRACT_PATH" ]]; then
    mkdir -p "$EXTRACT_PATH"
fi

if [[ -z "$OVERWRITE" && -n "$(ls -A "$EXTRACT_PATH" 2>/dev/null)" ]]; then
    echo "The extract path is not empty, exiting." >&2
    exit 1
fi

if [[ "$TARGET" == *.zst ]]; then
    if [[ -z "$ZSTD" ]]; then
       echo "Zstd binary not set, exiting" >&2
       exit 1
    fi
    "$ZSTD" -fcd "$TARGET" | tar -x -C "$EXTRACT_PATH"
else
    tar xf "$TARGET" -C "$EXTRACT_PATH"
fi
