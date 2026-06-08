#!/bin/sh
# this tests a bug in handling of - chars in Unlabeled args
# success  
./test_wrapper $srcdir/test61.out ../examples/test2 '-i 10 -s hello "-1 -1"'
