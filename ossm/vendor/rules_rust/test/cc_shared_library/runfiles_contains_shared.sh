#!/bin/sh

set -eux

# Validate that the runfiles directory actually contains the shared
# library against which the Rust binary is linked.
test -n "$(find ${RUNFILES_DIR} -name 'libshared.*')"
