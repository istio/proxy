#!/usr/bin/env bash

set -euo pipefail

if [[ -n "${BUILD_WORKSPACE_DIRECTORY:-}" ]]; then
    DOCS_WORKSPACE="${BUILD_WORKSPACE_DIRECTORY}"
else
    # https://stackoverflow.com/a/246128/7768383
    DOCS_WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
fi

pushd "${DOCS_WORKSPACE}" &> /dev/null
# It's important to clean the workspace so we don't end up with unintended
# docs artifacts in the new commit.
bazel clean \
&& bazel build //... \
&& cp bazel-bin/*.md ./src/ \
&& chmod 0644 ./src/*.md

if [[ -z "${SKIP_COMMIT:-}" ]]; then
    git add src/*.md && git commit -m "Regenerate documentation"
fi

popd &> /dev/null
