#!/usr/bin/env bash

set -o errexit -o nounset -o pipefail

# Passed as argument when invoking the script.
TAG="${1}"

# The prefix is chosen to match what GitHub generates for source archives
# This guarantees that users can easily switch from a released artifact to a source archive
# with minimal differences in their code (e.g. strip_prefix remains the same)
PREFIX="rules_pkg--${TAG:1}"
ARCHIVE="rules_pkg-$TAG.tar.gz"

bazel build distro:relnotes
cat bazel-bin/distro/relnotes.txt
