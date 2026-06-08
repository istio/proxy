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

"""The sh_configure module extension."""

load("@bazel_features//:features.bzl", "bazel_features")
load("//shell/private/repositories:sh_config.bzl", "sh_config")

def _sh_configure_impl(module_ctx):
    sh_config(name = "local_config_shell")
    if bazel_features.external_deps.extension_metadata_has_reproducible:
        return module_ctx.extension_metadata(reproducible = True)
    return None

sh_configure = module_extension(implementation = _sh_configure_impl)
