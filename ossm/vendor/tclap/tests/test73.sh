#!/bin/sh
# success  tests whether * in UnlabeledValueArg passes 
./test_wrapper $srcdir/test73.out ../examples/test2 '-i 1 -s asdf fff*fff'
