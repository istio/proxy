#!/bin/sh
# failure
# This used to fail on the "Too many arguments!" but now fails sooner,
# and on a more approriate error.
./test_wrapper $srcdir/test57.out ../examples/test5 '--aaa asdf -c fdas --fff blah -i one -i two -j huh'
