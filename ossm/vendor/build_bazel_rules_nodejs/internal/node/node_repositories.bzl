# Copyright 2017 The Bazel Authors. All rights reserved.
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

"""Install NodeJS & Yarn

This is a set of repository rules for setting up hermetic copies of NodeJS and Yarn.
See https://docs.bazel.build/versions/main/skylark/repository_rules.html
"""

load("@bazel_skylib//lib:versions.bzl", "versions")
load("@rules_nodejs//nodejs:repositories.bzl", "nodejs_register_toolchains")
load("@rules_nodejs//nodejs:yarn_repositories.bzl", "yarn_repositories")

def node_repositories(**kwargs):
    """
    Wrapper macro around node_repositories_rule to call it for each platform.

    Also register bazel toolchains, and make other convenience repositories.

    Args:
      **kwargs: the documentation is generated from the node_repositories_rule, not this macro.
    """

    # Require that users update Bazel, so that we don't need to support older ones.
    versions.check("4.0.0")

    # Back-compat: allow yarn_repositories args to be provided to node_repositories
    yarn_args = {}
    yarn_name = kwargs.pop("yarn_repository_name", "yarn")
    for k, v in kwargs.items():
        if k.startswith("yarn"):
            yarn_args[k] = kwargs.pop(k)
    yarn_repositories(
        name = yarn_name,
        **yarn_args
    )

    # Install new toolchain under "nodejs" repository name prefix
    nodejs_register_toolchains(name = "nodejs", **kwargs)
