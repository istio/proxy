#!/bin/sh
# Verifies that argv[1] is an empty file.

set -eux

test -f $1
test ! -s $1
