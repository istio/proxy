#!/bin/bash
set eux

FOO="foo "

# Example (warning): SC2086: Double quote to prevent globbing and word splitting.
if [ $FOO = "foo" ]; then
    echo "FOO is foo"
fi
