#/usr/bin/env bash
set -e -u -x -o pipefail
cd $(git rev-parse --show-toplevel)/internal/npm_install
tsc
rm index.js
mv generate_build_file.js index.js
