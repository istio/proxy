#!/usr/bin/env bash

set -e

bazel test --config=ci //...

rm -rf go/xds go/udpa

tools/generate_lang_files_from_protos.py

git add go/xds go/udpa

echo "If this check fails, apply following diff:"
git diff HEAD
git diff HEAD --quiet
