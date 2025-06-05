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
"""Module extension for cc auto configuration."""

load("@bazel_features//:features.bzl", "bazel_features")
load("//cc/private/toolchain:cc_configure.bzl", "cc_autoconf", "cc_autoconf_toolchains")

def _cc_configure_extension_impl(ctx):
    cc_autoconf_toolchains(name = "local_config_cc_toolchains")
    cc_autoconf(name = "local_config_cc")
    if bazel_features.external_deps.extension_metadata_has_reproducible:
        return ctx.extension_metadata(reproducible = True)
    else:
        return None

cc_configure_extension = module_extension(implementation = _cc_configure_extension_impl)
