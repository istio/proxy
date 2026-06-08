#!/bin/sh
# failure  validates that the correct error message
# is displayed for XOR'd args
./test_wrapper $srcdir/test75.out ../examples/test20 '-b -a'
