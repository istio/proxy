#!/usr/bin/env sh
# This script is a trivial to build replacement for the LCOV coverage merge tool
# shipped with Bazel in situations where code coverage is not being collected.
# It prevents the build time overhead and Java toolchain requirement incurred by
# the real tool when it is not needed.
exit 0
