# Copyright 2018 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Definitions for handling Bazel transitive repositories used by the 
dependencies of the Swift rules.
"""

load("@bazel_features//:deps.bzl", "bazel_features_deps")
load("@build_bazel_apple_support//lib:repositories.bzl", "apple_support_dependencies")
load(
    "@rules_proto//proto:repositories.bzl",
    "rules_proto_dependencies",
    "rules_proto_toolchains",
)

def swift_rules_extra_dependencies():
    """Fetches transitive repositories of the dependencies of `rules_swift`.

    Users should call this macro in their `WORKSPACE` following the use of
    `swift_rules_dependencies` to ensure that all of the dependencies of
    the Swift rules are downloaded and that they are isolated from changes
    to those dependencies.
    """

    apple_support_dependencies()

    rules_proto_dependencies()

    rules_proto_toolchains()

    bazel_features_deps()
