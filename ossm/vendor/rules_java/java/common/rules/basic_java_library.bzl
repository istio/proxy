# Copyright 2021 The Bazel Authors. All rights reserved.
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
Common code for reuse across java_* rules
"""

load("//java/common:java_common.bzl", "java_common")
load("//java/common:java_plugin_info.bzl", "JavaPluginInfo")
load("//java/common:java_semantics.bzl", "semantics")
load(":rule_util.bzl", "merge_attrs")

visibility([
    "//java/...",
])

BASIC_JAVA_LIBRARY_IMPLICIT_ATTRS = merge_attrs(
    {
        "_java_plugins": attr.label(
            default = semantics.JAVA_PLUGINS_FLAG_ALIAS_LABEL,
            providers = [JavaPluginInfo],
        ),
        # TODO(b/245144242): Used by IDE integration, remove when toolchains are used
        "_java_toolchain": attr.label(
            default = semantics.JAVA_TOOLCHAIN_LABEL,
            providers = [java_common.JavaToolchainInfo],
        ),
        "_use_auto_exec_groups": attr.bool(default = True),
    },
)
