# Copyright 2025 Google LLC
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

load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

licenses(["notice"])  # Apache 2

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "srcs",
    srcs = glob(["**"]),
)

cmake(
    name = "wasmedge_lib",
    cache_entries = {
        "WASMEDGE_BUILD_AOT_RUNTIME": "Off",
        "WASMEDGE_BUILD_SHARED_LIB": "Off",
        "WASMEDGE_BUILD_STATIC_LIB": "On",
        "WASMEDGE_BUILD_TOOLS": "Off",
        "WASMEDGE_FORCE_DISABLE_LTO": "On",
    },
    env = {
        "CXXFLAGS": "-Wno-error=dangling-reference -Wno-error=maybe-uninitialized -Wno-error=array-bounds= -Wno-error=deprecated-declarations -std=c++20",
    },
    generate_args = ["-GNinja"],
    lib_source = ":srcs",
    out_static_libs = ["libwasmedge.a"],
)
