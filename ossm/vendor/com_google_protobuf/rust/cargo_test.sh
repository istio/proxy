#!/bin/bash
# Runs tests that are defined in the protobuf crate using Cargo.
# This is not a hermetic task because Cargo will fetch the needed
# dependencies from crates.io

# --- begin runfiles.bash initialization ---
# Copy-pasted from Bazel's Bash runfiles library (tools/bash/runfiles/runfiles.bash).
set -euo pipefail
if [[ ! -d "${RUNFILES_DIR:-/dev/null}" && ! -f "${RUNFILES_MANIFEST_FILE:-/dev/null}" ]]; then
    if [[ -f "$0.runfiles_manifest" ]]; then
    export RUNFILES_MANIFEST_FILE="$0.runfiles_manifest"
    elif [[ -f "$0.runfiles/MANIFEST" ]]; then
    export RUNFILES_MANIFEST_FILE="$0.runfiles/MANIFEST"
    elif [[ -f "$0.runfiles/bazel_tools/tools/bash/runfiles/runfiles.bash" ]]; then
    export RUNFILES_DIR="$0.runfiles"
    fi
fi
if [[ -f "${RUNFILES_DIR:-/dev/null}/bazel_tools/tools/bash/runfiles/runfiles.bash" ]]; then
    source "${RUNFILES_DIR}/bazel_tools/tools/bash/runfiles/runfiles.bash"
elif [[ -f "${RUNFILES_MANIFEST_FILE:-/dev/null}" ]]; then
    source "$(grep -m1 "^bazel_tools/tools/bash/runfiles/runfiles.bash " \
            "$RUNFILES_MANIFEST_FILE" | cut -d ' ' -f 2-)"
else
    echo >&2 "ERROR: cannot find @bazel_tools//tools/bash/runfiles:runfiles.bash"
    exit 1
fi
# --- end runfiles.bash initialization ---

TMP_DIR=$(mktemp -d)
trap 'rm -rf -- "$TMP_DIR"' EXIT

CARGO_HOME=$TMP_DIR/cargo_home
mkdir $CARGO_HOME

CRATE_ROOT=$TMP_DIR/protobuf
mkdir $CRATE_ROOT

PROTOBUF_ZIP=$(rlocation com_google_protobuf/rust/rust_crate.zip)

unzip -d $CRATE_ROOT $PROTOBUF_ZIP

CODEGEN_ROOT=$TMP_DIR/protobuf_codegen
mkdir $CODEGEN_ROOT

CODEGEN_ZIP=$(rlocation com_google_protobuf/rust/codegen_crate.zip)

unzip -d $CODEGEN_ROOT $CODEGEN_ZIP

EXAMPLE_ROOT=$TMP_DIR/codegen_example
mkdir $EXAMPLE_ROOT

EXAMPLE_ZIP=$(rlocation com_google_protobuf/rust/codegen_example.zip)

unzip -d $EXAMPLE_ROOT $EXAMPLE_ZIP

cd $CRATE_ROOT
# Run all tests except doctests
CARGO_HOME=$CARGO_HOME cargo test --lib --bins --tests

cd $CODEGEN_ROOT
CARGO_HOME=$CARGO_HOME cargo test --lib --bins --tests

PROTOC=$(rlocation com_google_protobuf/protoc)
PROTOC_GEN_UPB_MINITABLE=$(rlocation com_google_protobuf/upb_generator/minitable/protoc-gen-upb_minitable)

cd $EXAMPLE_ROOT
CARGO_HOME=$CARGO_HOME PROTOC=$PROTOC PROTOC_GEN_UPB_MINITABLE=$PROTOC_GEN_UPB_MINITABLE cargo test
