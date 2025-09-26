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

"""Helper function for repository rules
"""

load(":node_versions.bzl", "NODE_VERSIONS")

OS_ARCH_NAMES = [
    ("windows", "amd64"),
    ("darwin", "amd64"),
    ("darwin", "arm64"),
    ("linux", "amd64"),
    ("linux", "arm64"),
    ("linux", "s390x"),
    ("linux", "ppc64le"),
    ("freebsd", "amd64"),
]

def os_name(rctx):
    """Get the os name for a repository rule

    Args:
      rctx: The repository rule context

    Returns:
      A string describing the os for a repository rule
    """
    if is_windows_os(rctx):
        return "windows_amd64"

    arch = rctx.os.arch
    if arch == "aarch64":
        arch = "arm64"

    # Java os macos is reporting x86_64.
    # See: https://mail.openjdk.org/pipermail/macosx-port-dev/2012-February/002860.html .
    if arch == "x86_64":
        arch = "amd64"

    os_name = rctx.os.name
    if os_name.startswith("mac os"):
        os_name = "darwin"
    elif os_name.startswith("linux"):
        os_name = "linux"
    elif os_name.startswith("freebsd"):
        os_name = "freebsd"

    os_and_arch = (os_name, arch)
    if os_and_arch not in OS_ARCH_NAMES:
        fail("Unsupported operating system {} architecture {}".format(os_name, arch))

    return "_".join(os_and_arch)

def is_windows_os(rctx):
    return rctx.os.name.find("windows") != -1

def node_exists_for_os(node_version, os_name, node_repositories):
    if not node_repositories:
        node_repositories = NODE_VERSIONS

    return "-".join([node_version, os_name]) in node_repositories.keys()

def assert_node_exists_for_host(rctx):
    node_version = rctx.attr.node_version
    node_repositories = rctx.attr.node_repositories

    if not node_exists_for_os(node_version, os_name(rctx), node_repositories):
        fail("No nodejs is available for {} at version {}".format(os_name(rctx), node_version) +
             "\n    Consider upgrading by setting node_version in a call to node_repositories in WORKSPACE." +
             "\n    Note that Node 16.x is the minimum published for Apple Silicon (M1 Macs)")
