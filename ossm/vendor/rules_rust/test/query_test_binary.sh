#!/bin/bash

set -euo pipefail

cd "${BUILD_WORKSPACE_DIRECTORY}"

# We always expect to be able to query ... in this repo with no flags.
# In the past this has regressed, e.g. in https://github.com/bazelbuild/rules_rust/issues/2359, so we have this test to ensure we don't regress again.
bazel query //... >/dev/null
bazel cquery //... >/dev/null
