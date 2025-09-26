#!/usr/bin/env bash
# See https://bazel.build/docs/user-manual#workspace-status

# Starts with the STABLE_ prefix so that Bazel will always rebuild stamped outputs if the git tag changes.
echo STABLE_BUILD_VERSION "$(git describe --tags --long | sed -e 's/^v//;s/-/./;s/-g/+/')"
