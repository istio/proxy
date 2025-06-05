#!/bin/sh
# this tests whether all required args are listed as
# missing when no arguments are specified
# failure  
./test_wrapper $srcdir/test67.out ../examples/test12 '-v "a 1 0.3"'
