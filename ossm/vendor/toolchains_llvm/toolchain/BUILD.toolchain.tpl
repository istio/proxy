# Copyright 2018 The Bazel Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

package(default_visibility = ["//visibility:public"])

load("@bazel_skylib//rules:native_binary.bzl", "native_binary")
load("@rules_cc//cc:defs.bzl", "cc_import", "cc_toolchain", "cc_toolchain_suite")
load("@toolchains_llvm//toolchain/internal:system_module_map.bzl", "system_module_map")
load("%{cc_toolchain_config_bzl}", "cc_toolchain_config")

# This filegroup target is used when not using absolute paths and shared
# between different toolchains.

# Tools wrapped and symlinked through this repo. This target is for internal use in the toolchain only.
filegroup(
    name = "internal-use-tools",
    srcs = ["%{tools_dir}"],
    visibility = ["//visibility:private"],
)

filegroup(
    name = "internal-use-tools-legacy",
    srcs = [%{symlinked_tools}
        "%{tools_dir}/cc_wrapper.sh",
    ],
    visibility = ["//visibility:private"],
)

%{cc_toolchains}

# Convenience targets from the LLVM toolchain.
%{convenience_targets}
