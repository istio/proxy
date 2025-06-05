# Copyright 2020 The Bazel Authors. All rights reserved.
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

load("//go:def.bzl", "go_context")

def _no_context_info_impl(ctx):
    go_context(ctx)
    # do nothing and pass if that succeeds

no_context_info = rule(
    implementation = _no_context_info_impl,
    toolchains = ["@io_bazel_rules_go//go:toolchain"],
)
