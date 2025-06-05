# Copyright 2021 The Bazel Authors. All rights reserved.
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
"""Module extensions for rules_java."""

load("@bazel_features//:features.bzl", "bazel_features")
load(
    "//java:repositories.bzl",
    "java_tools_repos",
    "local_jdk_repo",
    "remote_jdk11_repos",
    "remote_jdk17_repos",
    "remote_jdk21_repos",
    "remote_jdk8_repos",
)

def _toolchains_impl(module_ctx):
    java_tools_repos()
    local_jdk_repo()
    remote_jdk8_repos()
    remote_jdk11_repos()
    remote_jdk17_repos()
    remote_jdk21_repos()

    if bazel_features.external_deps.extension_metadata_has_reproducible:
        return module_ctx.extension_metadata(reproducible = True)
    else:
        return None

toolchains = module_extension(_toolchains_impl)
