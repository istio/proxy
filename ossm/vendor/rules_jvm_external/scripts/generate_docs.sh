#!/usr/bin/env bash

set -euo pipefail

bazel build //scripts:generate_api_reference && \
    cp bazel-bin/scripts/api.md docs/api.md && \
    chmod u+rw docs/api.md && \
    chmod a-x docs/api.md
