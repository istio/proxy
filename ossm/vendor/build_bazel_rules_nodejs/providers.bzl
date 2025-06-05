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

"""
Public providers, aspects and helpers that are shipped in the built-in build_bazel_rules_nodejs repository.

Note that many providers have moved to the rules_nodejs "core" package. See [Core API doc](./Core.md)

Users should not load files under "/internal"
"""

load(
    "//internal/providers:external_npm_package_info.bzl",
    _ExternalNpmPackageInfo = "ExternalNpmPackageInfo",
    _node_modules_aspect = "node_modules_aspect",
)
load(
    "//internal/providers:js_providers.bzl",
    _JSEcmaScriptModuleInfo = "JSEcmaScriptModuleInfo",
    _JSNamedModuleInfo = "JSNamedModuleInfo",
    _js_ecma_script_module_info = "js_ecma_script_module_info",
    _js_named_module_info = "js_named_module_info",
)
load(
    "//internal/providers:node_runtime_deps_info.bzl",
    _NodeRuntimeDepsInfo = "NodeRuntimeDepsInfo",
    _run_node = "run_node",
)

# TODO(6.0): remove these re-exports, they are just for easier migration to 5.0.0
# This includes everything from
# https://github.com/bazelbuild/rules_nodejs/blob/4.x/providers.bzl
# which wasn't removed in 5.0 (NodeContextInfo, NODE_CONTEXT_ATTRS)
load(
    "@rules_nodejs//nodejs:providers.bzl",
    _DeclarationInfo = "DeclarationInfo",
    _DirectoryFilePathInfo = "DirectoryFilePathInfo",
    _JSModuleInfo = "JSModuleInfo",
    _LinkablePackageInfo = "LinkablePackageInfo",
    _declaration_info = "declaration_info",
)

DeclarationInfo = _DeclarationInfo
declaration_info = _declaration_info
JSModuleInfo = _JSModuleInfo
LinkablePackageInfo = _LinkablePackageInfo
DirectoryFilePathInfo = _DirectoryFilePathInfo

ExternalNpmPackageInfo = _ExternalNpmPackageInfo
js_ecma_script_module_info = _js_ecma_script_module_info
js_named_module_info = _js_named_module_info
JSEcmaScriptModuleInfo = _JSEcmaScriptModuleInfo
JSNamedModuleInfo = _JSNamedModuleInfo
node_modules_aspect = _node_modules_aspect
NodeRuntimeDepsInfo = _NodeRuntimeDepsInfo
run_node = _run_node

# Export NpmPackageInfo for pre-3.0 legacy support in downstream rule sets
# such as rules_docker
# TODO(6.0): remove NpmPackageInfo from rules_docker & then remove it here
NpmPackageInfo = _ExternalNpmPackageInfo
