#!/bin/sh

# success - all unmatched args get slurped up in the UnlabeledMultiArg
./test_wrapper $srcdir/test82.out ../examples/test23 'blah --blah -s=bill -i=9 -i=8 -B homer marge bart'
