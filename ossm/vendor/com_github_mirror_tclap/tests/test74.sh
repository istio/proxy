#!/bin/sh
# failure  validates that the correct error message
# is displayed for XOR'd args
./test_wrapper $srcdir/test74.out ../examples/test20 '-a -b'
