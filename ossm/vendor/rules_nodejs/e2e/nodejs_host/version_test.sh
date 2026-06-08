#!/bin/bash

# --- begin runfiles.bash initialization v3 ---
# Copy-pasted from the Bazel Bash runfiles library v3.
set -uo pipefail; set +e; f=bazel_tools/tools/bash/runfiles/runfiles.bash
source "${RUNFILES_DIR:-/dev/null}/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "${RUNFILES_MANIFEST_FILE:-/dev/null}" | cut -f2- -d' ')" 2>/dev/null || \
  source "$0.runfiles/$f" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  source "$(grep -sm1 "^$f " "$0.exe.runfiles_manifest" | cut -f2- -d' ')" 2>/dev/null || \
  { echo>&2 "ERROR: cannot find $f"; exit 1; }; f=; set -e
# --- end runfiles.bash initialization v3 ---

# `--help --verbose` output looks like below - idea is to search within it for node version and path
#npm verb cli [
#npm verb cli   '/usr/local/bin/node',
#npm verb cli   '/private/var/tmp/_bazel_matt/bfec7e36d4722e7aa5af2167c4384e14/external/nodejs_darwin_arm64/bin/nodejs/lib/node_modules/npm/bin/npm-cli.js',
#npm verb cli   'exec',
#npm verb cli   '--loglevel',
#npm verb cli   'verbose'
#npm verb cli ]

# On Windows, there's a .cmd extension
EXT=${3:-}
NODE=$(rlocation "$1/node$EXT")
BIN=$(rlocation "$1/$2$EXT")

readonly EXPECTED_NODE_VERSION=$($NODE --version 2>&1)
readonly OUT=$($BIN --help --verbose 2>&1)

echo "-----"
echo "expected node version from node --version..."
echo $EXPECTED_NODE_VERSION
echo "$BIN --help --verbose stdout..."
echo ${OUT//\\//}
echo "-----"

if ! [[ $OUT =~ .*node@$EXPECTED_NODE_VERSION.* ]]; then
  echo "Error: No match on expected node version $EXPECTED_NODE_VERSION"
  exit 1
fi

exit 0
