# Copyright 2022 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

"""Load and configure transitive dependencies.

These are custom dependency loading functions provided by external
projects - due to bazel load formatting these cannot be loaded
in "build_deps.bzl".
"""

load(":riegeli_deps.bzl", "load_riegli_deps")
load("@pybind11_bazel//:python_configure.bzl", "python_configure")
load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
load("@com_google_ecclesia//ecclesia/build_defs:deps_first.bzl", "ecclesia_deps_first")

def load_secondary_deps():
    """Loads transitive dependencies of projects imported from build_deps.bzl"""
    load_riegli_deps()
    grpc_deps()
    protobuf_deps()
    rules_pkg_dependencies()
    ecclesia_deps_first()
    python_configure(name = "local_config_python", python_version = "3")
