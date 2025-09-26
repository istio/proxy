#!/usr/bin/env bash

set -euo pipefail

bazel build //scripts:generate_api_reference && \
    cp bazel-bin/scripts/api.md docs/api.md && \
    chmod u+rw docs/api.md && \
    chmod a-x docs/api.md

bazel build //docs:bzlmod && \
    cp bazel-bin/docs/bzlmod.md docs/bzlmod-api.md && \
    chmod u+rw docs/bzlmod-api.md && \
    chmod a-x docs/bzlmod-api.md
