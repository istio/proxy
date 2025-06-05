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

"""A resilient API layer wrapping compilation and other logic for Swift.

This module is meant to be used by custom rules that need to compile Swift code
and cannot simply rely on writing a macro that wraps `swift_library`. For
example, `swift_proto_library` generates Swift source code from `.proto` files
and then needs to compile them. This module provides that lower-level interface.
"""

load(
    "//swift/internal:attrs.bzl",
    "swift_compilation_attrs",
    "swift_library_rule_attrs",
    "swift_toolchain_attrs",
)
load(
    "//swift/internal:compiling.bzl",
    "compile",
    "compile_module_interface",
    "create_compilation_context",
    "precompile_clang_module",
)
load(
    "//swift/internal:features.bzl",
    "configure_features",
    "get_cc_feature_configuration",
    "is_feature_enabled",
)
load(
    "//swift/internal:linking.bzl",
    "create_linking_context_from_compilation_outputs",
)
load(
    "//swift/internal:symbol_graph_extracting.bzl",
    "extract_symbol_graph",
)
load(
    "//swift/internal:toolchain_utils.bzl",
    "get_swift_toolchain",
    "use_swift_toolchain",
)
load(":module_name.bzl", "derive_swift_module_name")
load(
    ":providers.bzl",
    "SwiftInfo",
    "create_clang_module_inputs",
    "create_swift_module_context",
    "create_swift_module_inputs",
)
load(":swift_interop_info.bzl", "create_swift_interop_info")

# The exported `swift_common` module, which defines the public API for directly
# invoking actions that compile Swift code from other rules.
swift_common = struct(
    cc_feature_configuration = get_cc_feature_configuration,
    compilation_attrs = swift_compilation_attrs,
    compile = compile,
    compile_module_interface = compile_module_interface,
    configure_features = configure_features,
    # TODO(b/261445197): Remove this after everyone is migrated to the free
    # function.
    create_clang_module = create_clang_module_inputs,
    create_compilation_context = create_compilation_context,
    create_linking_context_from_compilation_outputs = create_linking_context_from_compilation_outputs,
    # TODO(b/261445197): Remove this after everyone is migrated to the free
    # function.
    create_module = create_swift_module_context,
    # TODO(b/261445197): Remove this after everyone is migrated to the free
    # function.
    create_swift_info = SwiftInfo,
    # TODO(b/261445197): Remove this after everyone is migrated to the free
    # function.
    create_swift_interop_info = create_swift_interop_info,
    # TODO(b/261445197): Remove this after everyone is migrated to the free
    # function.
    create_swift_module = create_swift_module_inputs,
    # TODO(b/261444771): Remove this after everyone is migrated to the free
    # function.
    derive_module_name = derive_swift_module_name,
    extract_symbol_graph = extract_symbol_graph,
    get_toolchain = get_swift_toolchain,
    is_enabled = is_feature_enabled,
    library_rule_attrs = swift_library_rule_attrs,
    precompile_clang_module = precompile_clang_module,
    toolchain_attrs = swift_toolchain_attrs,
    use_toolchain = use_swift_toolchain,
)
