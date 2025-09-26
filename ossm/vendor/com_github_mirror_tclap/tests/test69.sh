#!/bin/sh
# Checks that parsing exceptions are properly
# propagated to the caller.
./test_wrapper $srcdir/test69.out ../examples/test18 '--bob'
