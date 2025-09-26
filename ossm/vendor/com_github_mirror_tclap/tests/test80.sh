#!/bin/sh

# success (everything but -n mike should be ignored)
./test_wrapper $srcdir/test80.out ../examples/test22 'asdf -n mike asdf fds xxx'
