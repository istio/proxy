#!/bin/sh
# Checks that parsing exceptions are properly
# propagated to the caller.
./test_wrapper $srcdir/test70.out ../examples/test18 '--help'
