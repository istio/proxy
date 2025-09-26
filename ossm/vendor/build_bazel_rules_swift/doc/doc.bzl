# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""Bazel rules to define Swift libraries and executable binaries.

Users should load these rules from `.bzl` files under the `swift` and `proto`
directories. Do not import definitions from the `internal` subdirectory
directly.

For example:

```build
load("@build_bazel_rules_swift//swift:swift_library.bzl", "swift_library")
load("@build_bazel_rules_swift//proto:swift_proto_library.bzl", "swift_proto_library")
```
"""

load(
    "//mixed_language:mixed_language_library.bzl",
    _mixed_language_library = "mixed_language_library",
)
load(
    "//proto:swift_proto_common.bzl",
    _swift_proto_common = "swift_proto_common",
)
load(
    "//proto:swift_proto_compiler.bzl",
    _swift_proto_compiler = "swift_proto_compiler",
)
load(
    "//proto:swift_proto_library.bzl",
    _swift_proto_library = "swift_proto_library",
)
load(
    "//proto:swift_proto_library_group.bzl",
    _swift_proto_library_group = "swift_proto_library_group",
)
load(
    "//swift:module_name.bzl",
    _derive_swift_module_name = "derive_swift_module_name",
)
load(
    "//swift:providers.bzl",
    _SwiftInfo = "SwiftInfo",
    _SwiftProtoCompilerInfo = "SwiftProtoCompilerInfo",
    _SwiftProtoInfo = "SwiftProtoInfo",
    _SwiftToolchainInfo = "SwiftToolchainInfo",
)
load("//swift:swift_binary.bzl", _swift_binary = "swift_binary")
load("//swift:swift_common.bzl", _swift_common = "swift_common")
load(
    "//swift:swift_compiler_plugin.bzl",
    _swift_compiler_plugin = "swift_compiler_plugin",
    _universal_swift_compiler_plugin = "universal_swift_compiler_plugin",
)
load(
    "//swift:swift_compiler_plugin_import.bzl",
    _swift_compiler_plugin_import = "swift_compiler_plugin_import",
)
load(
    "//swift:swift_cross_import_overlay.bzl",
    _swift_cross_import_overlay = "swift_cross_import_overlay",
)
load(
    "//swift:swift_feature_allowlist.bzl",
    _swift_feature_allowlist = "swift_feature_allowlist",
)
load("//swift:swift_import.bzl", _swift_import = "swift_import")
load(
    "//swift:swift_interop_hint.bzl",
    _swift_interop_hint = "swift_interop_hint",
)
load("//swift:swift_library.bzl", _swift_library = "swift_library")
load(
    "//swift:swift_library_group.bzl",
    _swift_library_group = "swift_library_group",
)
load(
    "//swift:swift_module_alias.bzl",
    _swift_module_alias = "swift_module_alias",
)
load(
    "//swift:swift_module_mapping.bzl",
    _swift_module_mapping = "swift_module_mapping",
)
load(
    "//swift:swift_module_mapping_test.bzl",
    _swift_module_mapping_test = "swift_module_mapping_test",
)
load(
    "//swift:swift_package_configuration.bzl",
    _swift_package_configuration = "swift_package_configuration",
)
load("//swift:swift_test.bzl", _swift_test = "swift_test")

# The following are re-exported symbols for consumption from stardoc.

# proto symbols
swift_proto_common = _swift_proto_common
SwiftProtoCompilerInfo = _SwiftProtoCompilerInfo
SwiftProtoInfo = _SwiftProtoInfo
swift_proto_compiler = _swift_proto_compiler
swift_proto_library = _swift_proto_library
swift_proto_library_group = _swift_proto_library_group

# swift symbols
derive_swift_module_name = _derive_swift_module_name
swift_common = _swift_common
SwiftInfo = _SwiftInfo
SwiftToolchainInfo = _SwiftToolchainInfo
swift_binary = _swift_binary
swift_compiler_plugin = _swift_compiler_plugin
universal_swift_compiler_plugin = _universal_swift_compiler_plugin
swift_compiler_plugin_import = _swift_compiler_plugin_import
swift_cross_import_overlay = _swift_cross_import_overlay
swift_feature_allowlist = _swift_feature_allowlist
swift_import = _swift_import
swift_interop_hint = _swift_interop_hint
swift_library = _swift_library
swift_library_group = _swift_library_group
mixed_language_library = _mixed_language_library
swift_module_alias = _swift_module_alias
swift_module_mapping = _swift_module_mapping
swift_module_mapping_test = _swift_module_mapping_test
swift_package_configuration = _swift_package_configuration
swift_test = _swift_test
