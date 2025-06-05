#!/bin/bash

set -e # Quit on error
#set -x # Echo commands

function status {
    cmake -E cmake_echo_color --blue "$1"
}

function warn {
    cmake -E cmake_echo_color --yellow "$1"
}

function error {
    cmake -E cmake_echo_color --red "$1"
    exit 1
}


#
# Get command line args
#
CMAKE_CURRENT_SOURCE_DIR="${1?"CMAKE_CURRENT_SOURCE_DIR not specified"}"
CMAKE_CURRENT_BINARY_DIR="${2?"CMAKE_CURRENT_BINARY_DIR not specified"}"
SRC_FILE="${3?"SRC_FILE not specified"}" # e.g. crypto/err/internal.h
DST_FILE="${4?"DST_FILE not specified"}" # e.g. source/crypto/err/internal.h

SRC_DIR="$CMAKE_CURRENT_SOURCE_DIR/third_party/boringssl/src"
PATCH_DIR="$CMAKE_CURRENT_SOURCE_DIR/patch"

#
# Check/Ensure the inputs and outputs exist
#
[[ -d "$SRC_DIR" ]] || error "SRC_DIR $SRC_DIR does not exist"
[[ -f "$SRC_DIR/$SRC_FILE" ]] || error "SRC_FILE $SRC_FILE does not exist in $SRC_DIR"
[[ -d "$PATCH_DIR" ]] || error "PATCH_DIR $PATCH_DIR does not exist"
mkdir -p "$(dirname "$CMAKE_CURRENT_BINARY_DIR/$DST_FILE")"


#
# Apply script file from $PATCH_DIR
# =================================
#
PATCH_SCRIPT="$PATCH_DIR/$DST_FILE.sh"
GEN_APPLIED_SCRIPT="$CMAKE_CURRENT_BINARY_DIR/$DST_FILE.1.applied.script"
cp "$SRC_DIR/$SRC_FILE" "$GEN_APPLIED_SCRIPT"
if [ -f "$PATCH_SCRIPT" ]; then
    PATH="$(dirname "$0"):$PATH" "$PATCH_SCRIPT" "$GEN_APPLIED_SCRIPT"
else # Comment out the whole file contents
    "$(dirname "$0")/uncomment.sh" "$GEN_APPLIED_SCRIPT" --comment
fi


#
# Apply patch file from $PATCH_DIR
# ================================
#
PATCH_FILE="$PATCH_DIR/$DST_FILE.patch"
GEN_APPLIED_PATCH="$CMAKE_CURRENT_BINARY_DIR/$DST_FILE.2.applied.patch"
if [ -f "$PATCH_FILE" ]; then
    patch -s -f "$GEN_APPLIED_SCRIPT" "$PATCH_FILE" -o "$GEN_APPLIED_PATCH"
else
    cp "$GEN_APPLIED_SCRIPT" "$GEN_APPLIED_PATCH"
fi


#
# Copy result to the destination
# ==============================
#
cp "$GEN_APPLIED_PATCH" "$CMAKE_CURRENT_BINARY_DIR/$DST_FILE"
