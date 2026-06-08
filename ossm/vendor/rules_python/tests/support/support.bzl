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

load("@rules_python_internal//:rules_python_config.bzl", "config")
load("//python/private:bzlmod_enabled.bzl", "BZLMOD_ENABLED")  # buildifier: disable=bzl-visibility

PY_TOOLCHAINS = str(Label("//tests/support/py_toolchains:all"))
CC_TOOLCHAIN = str(Label("//tests/support/cc_toolchains:all"))
CROSSTOOL_TOP = Label("//tests/support/cc_toolchains:cc_toolchain_suite")

# str() around Label() is necessary because rules_testing's config_settings
# doesn't accept yet Label objects.
CUSTOM_RUNTIME = str(Label("//tests/support:custom_runtime"))

SUPPORTS_BOOTSTRAP_SCRIPT = select({
    "@platforms//os:windows": ["@platforms//:incompatible"],
    "//conditions:default": [],
})

SUPPORTS_BZLMOD_UNIXY = select({
    "@platforms//os:windows": ["@platforms//:incompatible"],
    "//conditions:default": [],
}) if BZLMOD_ENABLED else ["@platforms//:incompatible"]

NOT_WINDOWS = select({
    "@platforms//os:windows": ["@platforms//:incompatible"],
    "//conditions:default": [],
})

BAZEL_8_OR_LATER = [] if config.bazel_8_or_later else ["@platforms//:incompatible"]
