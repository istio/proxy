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

# Utilities for protocol buffers.
#
# https://docs.bazel.build/versions/master/skylark/lib/proto_common.html
"""proto_common module"""

load("@com_google_protobuf//bazel/common:proto_common.bzl", _proto_common = "proto_common")
load("@com_google_protobuf//bazel/common:proto_lang_toolchain_info.bzl", _ProtoLangToolchainInfo = "ProtoLangToolchainInfo")

# Deprecated: use protobuf directly.
proto_common = _proto_common  # reexport proto_common for current users

# Deprecated: use protobuf directly.
ProtoLangToolchainInfo = _ProtoLangToolchainInfo

def _incompatible_toolchains_enabled():
    return getattr(proto_common, "INCOMPATIBLE_ENABLE_PROTO_TOOLCHAIN_RESOLUTION", False)

def _find_toolchain(ctx, legacy_attr, toolchain_type):
    if _incompatible_toolchains_enabled():
        toolchain = ctx.toolchains[toolchain_type]
        if not toolchain:
            fail("No toolchains registered for '%s'." % toolchain_type)
        return toolchain.proto
    else:
        return getattr(ctx.attr, legacy_attr)[ProtoLangToolchainInfo]

def _use_toolchain(toolchain_type):
    if _incompatible_toolchains_enabled():
        return [config_common.toolchain_type(toolchain_type, mandatory = False)]
    else:
        return []

def _if_legacy_toolchain(legacy_attr_dict):
    if _incompatible_toolchains_enabled():
        return {}
    else:
        return legacy_attr_dict

toolchains = struct(
    use_toolchain = _use_toolchain,
    find_toolchain = _find_toolchain,
    if_legacy_toolchain = _if_legacy_toolchain,
)
