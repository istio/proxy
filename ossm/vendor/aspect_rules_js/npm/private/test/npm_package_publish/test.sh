#!/usr/bin/env bash

readonly PUBLISH_A="$1"
readonly PUBLISH_B="$2"

# assert that it prints package name from package.json to stderr,
# to ensure package directory is properly passed and npm can read it.
$PUBLISH_A 2>&1 | grep 'npm notice package: @mycorp/pkg-b@'

# shellcheck disable=SC2181
if [ $? != 0 ]; then
    echo "FAIL: expected 'npm notice package: @mycorp/pkg-b@' error"
    exit 1
fi

# asserting that npm_package has no package.json in it's srcs and we fail correctly.
# npm publish requires a package.json in the root of the package directory.
$PUBLISH_B 2>&1 | grep 'npm ERR! enoent This is related to npm not being able to find a file.'

# shellcheck disable=SC2181
if [ $? != 0 ]; then
    echo "FAIL: expected 'npm ERR! enoent This is related to npm not being able to find a file.' error"
    exit 1
fi
