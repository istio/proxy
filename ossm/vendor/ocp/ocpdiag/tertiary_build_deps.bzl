# Copyright 2022 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

"""Load and configure gRPC extra dependencies.

These are custom dependency loading functions provided by external
projects - due to bazel load formatting these cannot be loaded
in "build_deps.bzl".
"""

load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")
load("@com_google_ecclesia//ecclesia/build_defs:deps_second.bzl", "ecclesia_deps_second")

def load_tertiary_deps():
    """Loads tertiary dependencies of gRPC (deps of gRPC that also have deps)"""
    grpc_extra_deps()
    ecclesia_deps_second()
