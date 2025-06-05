# Copyright 2023 The Bazel Authors. All rights reserved.
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
Utilities for proto compiler rules.
"""

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("//proto:swift_proto_compiler.bzl", "swift_proto_compiler")

# NOTE: The ProtoPathModuleMappings option is set internally for all plugins.
# This is used to inform the plugins which Swift module the generated code for each plugin is located in.
PROTO_PLUGIN_OPTION_ALLOWLIST = [
    "FileNaming",
    "Visibility",
]
PROTO_PLUGIN_OPTIONS = {
    "Visibility": "Public",
}
GRPC_VARIANT_SERVER = "Server"
GRPC_VARIANT_CLIENT = "Client"
GRPC_VARIANT_TEST_CLIENT = "TestClient"
GRPC_VARIANTS = [
    GRPC_VARIANT_SERVER,
    GRPC_VARIANT_CLIENT,
    GRPC_VARIANT_TEST_CLIENT,
]
GRPC_PLUGIN_OPTION_ALLOWLIST = PROTO_PLUGIN_OPTION_ALLOWLIST + [
    "KeepMethodCasing",
    "ExtraModuleImports",
    "GRPCModuleName",
    "SwiftProtobufModuleName",
] + GRPC_VARIANTS

# NOTE: As of Swift 5.6, the TestClient flavor is deprecated in grpc-swift.
# This is because they are not sendable and needed to be marked as unchecked sendable for async/await.
# We might just want to drop support for it during this migration.

def make_grpc_swift_proto_compiler(
        name,
        variants,
        plugin_options = PROTO_PLUGIN_OPTIONS):
    """Generates a GRPC swift_proto_compiler target for the given variants.

    Args:
        name: The name of the generated swift proto compiler target.
        variants: The list of variants the compiler should generate.
        plugin_options: Additional options to pass to the plugin.
    """

    # Merge the plugin options to include the variants:
    merged_plugin_options = dicts.add(
        plugin_options,
        {variant: "false" for variant in GRPC_VARIANTS},
    )
    for variant in variants:
        merged_plugin_options[variant] = "true"

    swift_proto_compiler(
        name = name,
        protoc = "//tools/protoc_wrapper:protoc",
        plugin = "//tools/protoc_wrapper:protoc-gen-grpc-swift",
        plugin_name = name.removesuffix("_proto"),
        plugin_option_allowlist = GRPC_PLUGIN_OPTION_ALLOWLIST,
        plugin_options = merged_plugin_options,
        suffixes = [".grpc.swift"],
        deps = [
            "@com_github_apple_swift_protobuf//:SwiftProtobuf",
            "@com_github_grpc_grpc_swift//:GRPC",
        ],
        visibility = ["//visibility:public"],
    )
