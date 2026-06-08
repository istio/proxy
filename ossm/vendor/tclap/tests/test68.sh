#!/bin/sh
# this tests whether we can parse args from
# a vector of strings and that combined switch
# handling doesn't get fooled if the delimiter
# is in the string
# success  
./test_wrapper $srcdir/test68.out ../examples/test13
