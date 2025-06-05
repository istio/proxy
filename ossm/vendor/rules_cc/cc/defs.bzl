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
"""Starlark rules for building C++ projects."""

load("@com_google_protobuf//bazel:cc_proto_library.bzl", _cc_proto_library = "cc_proto_library")
load("//cc:cc_binary.bzl", _cc_binary = "cc_binary")
load("//cc:cc_import.bzl", _cc_import = "cc_import")
load("//cc:cc_library.bzl", _cc_library = "cc_library")
load("//cc:cc_shared_library.bzl", _cc_shared_library = "cc_shared_library")
load("//cc:cc_test.bzl", _cc_test = "cc_test")
load("//cc:objc_import.bzl", _objc_import = "objc_import")
load("//cc:objc_library.bzl", _objc_library = "objc_library")
load("//cc/common:cc_common.bzl", _cc_common = "cc_common")
load("//cc/common:cc_info.bzl", _CcInfo = "CcInfo")
load("//cc/common:debug_package_info.bzl", _DebugPackageInfo = "DebugPackageInfo")
load("//cc/toolchains:cc_flags_supplier.bzl", _cc_flags_supplier = "cc_flags_supplier")
load("//cc/toolchains:cc_toolchain.bzl", _cc_toolchain = "cc_toolchain")
load("//cc/toolchains:cc_toolchain_config_info.bzl", _CcToolchainConfigInfo = "CcToolchainConfigInfo")
load("//cc/toolchains:cc_toolchain_suite.bzl", _cc_toolchain_suite = "cc_toolchain_suite")
load("//cc/toolchains:compiler_flag.bzl", _compiler_flag = "compiler_flag")
load("//cc/toolchains:fdo_prefetch_hints.bzl", _fdo_prefetch_hints = "fdo_prefetch_hints")
load("//cc/toolchains:fdo_profile.bzl", _fdo_profile = "fdo_profile")

# Rules

cc_library = _cc_library
cc_binary = _cc_binary
cc_test = _cc_test
cc_import = _cc_import
cc_shared_library = _cc_shared_library

objc_library = _objc_library
objc_import = _objc_import

# DEPRECATED: use rule from com_google_protobuf repository
def cc_proto_library(**kwargs):
    if "deprecation" not in kwargs:
        _cc_proto_library(deprecation = "Use cc_proto_library from com_google_protobuf", **kwargs)
    else:
        _cc_proto_library(**kwargs)

# Toolchain rules

cc_toolchain = _cc_toolchain
fdo_profile = _fdo_profile
fdo_prefetch_hints = _fdo_prefetch_hints
cc_toolchain_suite = _cc_toolchain_suite
compiler_flag = _compiler_flag
cc_flags_supplier = _cc_flags_supplier

# Modules and providers

cc_common = _cc_common
CcInfo = _CcInfo
DebugPackageInfo = _DebugPackageInfo
CcToolchainConfigInfo = _CcToolchainConfigInfo
