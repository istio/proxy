# Copyright 2019 The Bazel Authors. All rights reserved.
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
"""Starlark rules for building Java projects."""

load("//java:java_binary.bzl", _java_binary = "java_binary")
load("//java:java_import.bzl", _java_import = "java_import")
load("//java:java_library.bzl", _java_library = "java_library")
load("//java:java_plugin.bzl", _java_plugin = "java_plugin")
load("//java:java_test.bzl", _java_test = "java_test")
load("//java/common:java_common.bzl", _java_common = "java_common")
load("//java/common:java_info.bzl", _JavaInfo = "JavaInfo")
load("//java/common:java_plugin_info.bzl", _JavaPluginInfo = "JavaPluginInfo")
load("//java/toolchains:java_package_configuration.bzl", _java_package_configuration = "java_package_configuration")
load("//java/toolchains:java_runtime.bzl", _java_runtime = "java_runtime")
load("//java/toolchains:java_toolchain.bzl", _java_toolchain = "java_toolchain")

# Language rules

java_binary = _java_binary
java_test = _java_test
java_library = _java_library
java_plugin = _java_plugin
java_import = _java_import

# Toolchain rules

java_runtime = _java_runtime
java_toolchain = _java_toolchain
java_package_configuration = _java_package_configuration

# Proto rules
# Deprecated: don't use java proto libraries from here
java_proto_library = native.java_proto_library
java_lite_proto_library = native.java_lite_proto_library

# Modules and providers

JavaInfo = _JavaInfo
JavaPluginInfo = _JavaPluginInfo
java_common = _java_common
