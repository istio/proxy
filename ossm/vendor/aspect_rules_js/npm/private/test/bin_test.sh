#!/usr/bin/env bash
set -o errexit -o nounset

# put node on the path
node_path=$(dirname "$1")
if [[ "$node_path" == external/* ]]; then
    node_path="${node_path:9}"
fi
PATH="$PWD/../$node_path:$PATH"

node ./node_modules/typescript/bin/tsc --version
node ./npm/private/test/node_modules/typescript/bin/tsc --version

# test bin entries
./node_modules/.bin/tsc --version
./npm/private/test/node_modules/.bin/tsc --version
