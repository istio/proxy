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
"""Code that support testing of rules_python code."""

# NOTE: Explicit Label() calls are required so that it resolves in @rules_python
# context instead of e.g. the @rules_testing context.
# NOTE: Some labels require str() around Label() because they are passed onto
# rules_testing or as config_setting values, which don't support Label in some
# places.

load("//python/private:util.bzl", "IS_BAZEL_7_OR_HIGHER")  # buildifier: disable=bzl-visibility

MAC = Label("//tests/support:mac")
MAC_X86_64 = Label("//tests/support:mac_x86_64")
LINUX = Label("//tests/support:linux")
LINUX_X86_64 = Label("//tests/support:linux_x86_64")
WINDOWS = Label("//tests/support:windows")
WINDOWS_X86_64 = Label("//tests/support:windows_x86_64")

PY_TOOLCHAINS = str(Label("//tests/support/py_toolchains:all"))
CC_TOOLCHAIN = str(Label("//tests/support/cc_toolchains:all"))
CROSSTOOL_TOP = Label("//tests/support/cc_toolchains:cc_toolchain_suite")

# str() around Label() is necessary because rules_testing's config_settings
# doesn't accept yet Label objects.
ADD_SRCS_TO_RUNFILES = str(Label("//python/config_settings:add_srcs_to_runfiles"))
EXEC_TOOLS_TOOLCHAIN = str(Label("//python/config_settings:exec_tools_toolchain"))
PRECOMPILE = str(Label("//python/config_settings:precompile"))
PRECOMPILE_SOURCE_RETENTION = str(Label("//python/config_settings:precompile_source_retention"))
PYC_COLLECTION = str(Label("//python/config_settings:pyc_collection"))
PYTHON_VERSION = str(Label("//python/config_settings:python_version"))
VISIBLE_FOR_TESTING = str(Label("//python/private:visible_for_testing"))

SUPPORTS_BOOTSTRAP_SCRIPT = select({
    "@platforms//os:windows": ["@platforms//:incompatible"],
    "//conditions:default": [],
}) if IS_BAZEL_7_OR_HIGHER else ["@platforms//:incompatible"]
