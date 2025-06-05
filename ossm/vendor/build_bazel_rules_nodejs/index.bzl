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

"""Public API surface is re-exported here.

Users should not load files under "/internal"
"""

load("@rules_nodejs//nodejs:repositories.bzl", _nodejs_register_toolchains = "nodejs_register_toolchains")
load("@rules_nodejs//nodejs:yarn_repositories.bzl", _yarn_repositories = "yarn_repositories")
load("//:version.bzl", "VERSION")
load("//internal/common:check_version.bzl", "check_version")
load("//internal/common:copy_to_bin.bzl", _copy_to_bin = "copy_to_bin")
load("//internal/common:params_file.bzl", _params_file = "params_file")
load("//internal/generated_file_test:generated_file_test.bzl", _generated_file_test = "generated_file_test")
load("//internal/js_library:js_library.bzl", _js_library = "js_library")
load(
    "//internal/node:node.bzl",
    _nodejs_binary = "nodejs_binary_macro",
    _nodejs_test = "nodejs_test_macro",
)
load("//internal/node:node_repositories.bzl", _node_repositories = "node_repositories")
load("//internal/node:npm_package_bin.bzl", _npm_bin = "npm_package_bin")
load("//internal/npm_install:npm_install.bzl", _npm_install = "npm_install", _yarn_install = "yarn_install")
load("//internal/pkg_npm:pkg_npm.bzl", _pkg_npm = "pkg_npm_macro")
load("//internal/pkg_web:pkg_web.bzl", _pkg_web = "pkg_web")

nodejs_binary = _nodejs_binary
nodejs_test = _nodejs_test
node_repositories = _node_repositories
pkg_npm = _pkg_npm
npm_package_bin = _npm_bin
pkg_web = _pkg_web
copy_to_bin = _copy_to_bin
params_file = _params_file
generated_file_test = _generated_file_test
js_library = _js_library
# ANY RULES ADDED HERE SHOULD BE DOCUMENTED, see index.for_docs.bzl

# @unsorted-dict-items
COMMON_REPLACEMENTS = {
    # Make sure we don't try to load from under tools/ which isn't in the distro
    "(load\\(\"//:tools/defaults.bzl\", .*\\))": "# defaults.bzl not included in distribution\n# $1",
    # Cleanup up package.json @bazel/foobar package deps for published packages:
    # "@bazel/foobar": "0.0.0-PLACEHOLDER" => "@bazel/foobar": "0.0.0-PLACEHOLDER"
    "\"@bazel/([a-zA-Z_-]+)\":\\s+\"(file|bazel)[^\"]+\"": "\"@bazel/$1\": \"0.0.0-PLACEHOLDER\"",
}

def _maybe_install_node(**kwargs):
    # Just in case the user didn't install node, and if they didn't supply the name of the node repo,
    # then install a default one "@nodejs" for them here.
    if "node_repository" not in kwargs.keys() and "nodejs_toolchains" not in native.existing_rules():
        # buildifier: disable=print
        print("node_repository attribute not set and no repository named 'nodejs_toolchains' exists; installing default node")
        _nodejs_register_toolchains(name = "nodejs")

def _maybe_install_yarn(**kwargs):
    # Just in case the user didn't install yarn, and if they aren't using a custom yarn binary,
    # install the default "@yarn" one for them here.
    if "yarn" not in kwargs.keys() and "yarn" not in native.existing_rules():
        # buildifier: disable=print
        print("yarn_install#yarn attribute not set and no repository named 'yarn' exists; installing default yarn")
        _yarn_repositories(name = "yarn", node_repository = kwargs.get("node_repository"))

def npm_install(**kwargs):
    _maybe_install_node(**kwargs)
    _npm_install(**kwargs)

def yarn_install(**kwargs):
    _maybe_install_node(**kwargs)
    _maybe_install_yarn(**kwargs)
    _yarn_install(**kwargs)

# Currently used Bazel version. This version is what the rules here are tested
# against.
# This version should be updated together with the version of the Bazel
# in .bazelversion. This is asserted in //internal:bazel_version_test.
BAZEL_VERSION = "5.0.0"

# Versions of Bazel which users should be able to use.
# Ensures we don't break backwards-compatibility,
# accidentally forcing users to update their LTS-supported bazel.
# These are the versions used when testing nested workspaces with
# bazel_integration_test.
SUPPORTED_BAZEL_VERSIONS = ["4.2.2", BAZEL_VERSION]

def check_build_bazel_rules_nodejs_version(minimum_version_string):
    """
    Verify that a minimum build_bazel_rules_nodejs is loaded a WORKSPACE.

    This should be called from the `WORKSPACE` file so that the build fails as
    early as possible. For example:

    ```
    # in WORKSPACE:
    load("@build_bazel_rules_nodejs//:index.bzl", "check_build_bazel_rules_nodejs_version")
    check_build_bazel_rules_nodejs_version("4.4.0")
    ```

    Args:
      minimum_version_string: a string indicating the minimum version
    """
    if not check_version(VERSION, minimum_version_string):
        fail("\nCurrent build_bazel_rules_nodejs version is {}, expected at least {}\n".format(
            VERSION,
            minimum_version_string,
        ))

def check_rules_nodejs_version(*_):
    # buildifier: disable=print
    print("WARNING: check_rules_nodejs_version has been removed. This is a no-op, please remove the call.")
