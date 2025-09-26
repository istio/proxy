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

"""BUILD rules to define Swift libraries and executable binaries.

**NOTE:** This file is deprecated. To avoid having Bazel do more work than
necessary, users should import each rule/build definition they use from the
`.bzl` file that defines it in this directory.

Do not import any definitions directly from the `internal` directory; those are
meant for build rule use only.
"""

load(
    "@build_bazel_rules_swift//swift:swift_compiler_plugin.bzl",
    _swift_compiler_plugin = "swift_compiler_plugin",
    _universal_swift_compiler_plugin = "universal_swift_compiler_plugin",
)
load(
    ":providers.bzl",
    _SwiftInfo = "SwiftInfo",
    _SwiftProtoCompilerInfo = "SwiftProtoCompilerInfo",
    _SwiftProtoInfo = "SwiftProtoInfo",
    _SwiftSymbolGraphInfo = "SwiftSymbolGraphInfo",
    _SwiftToolchainInfo = "SwiftToolchainInfo",
)
load(":swift_binary.bzl", _swift_binary = "swift_binary")
load(
    ":swift_clang_module_aspect.bzl",
    _swift_clang_module_aspect = "swift_clang_module_aspect",
)
load(":swift_common.bzl", _swift_common = "swift_common")
load(
    ":swift_extract_symbol_graph.bzl",
    _swift_extract_symbol_graph = "swift_extract_symbol_graph",
)
load(
    ":swift_feature_allowlist.bzl",
    _swift_feature_allowlist = "swift_feature_allowlist",
)
load(":swift_import.bzl", _swift_import = "swift_import")
load(":swift_interop_hint.bzl", _swift_interop_hint = "swift_interop_hint")
load(":swift_library.bzl", _swift_library = "swift_library")
load(":swift_library_group.bzl", _swift_library_group = "swift_library_group")
load(":swift_module_alias.bzl", _swift_module_alias = "swift_module_alias")
load(
    ":swift_package_configuration.bzl",
    _swift_package_configuration = "swift_package_configuration",
)
load(
    ":swift_symbol_graph_aspect.bzl",
    _swift_symbol_graph_aspect = "swift_symbol_graph_aspect",
)
load(":swift_test.bzl", _swift_test = "swift_test")

# Re-export providers.
SwiftInfo = _SwiftInfo
SwiftProtoCompilerInfo = _SwiftProtoCompilerInfo
SwiftProtoInfo = _SwiftProtoInfo
SwiftSymbolGraphInfo = _SwiftSymbolGraphInfo
SwiftToolchainInfo = _SwiftToolchainInfo

# Re-export public API module.
swift_common = _swift_common

# Re-export rules.
swift_binary = _swift_binary
swift_compiler_plugin = _swift_compiler_plugin
universal_swift_compiler_plugin = _universal_swift_compiler_plugin
swift_extract_symbol_graph = _swift_extract_symbol_graph
swift_feature_allowlist = _swift_feature_allowlist
swift_import = _swift_import
swift_interop_hint = _swift_interop_hint
swift_library = _swift_library
swift_library_group = _swift_library_group
swift_module_alias = _swift_module_alias
swift_package_configuration = _swift_package_configuration
swift_test = _swift_test

# Re-export public aspects.
swift_clang_module_aspect = _swift_clang_module_aspect
swift_symbol_graph_aspect = _swift_symbol_graph_aspect
