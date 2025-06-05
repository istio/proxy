#!/bin/sh
# this tests whether all required args are listed as
# missing when no arguments are specified
# failure  
./test_wrapper $srcdir/test64.out ../examples/test11 '-v "1 2 3"'
