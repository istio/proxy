# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@bazel_skylib//lib:selects.bzl", "selects")
load(":v8_transition.bzl", "v8_lib_no_pointer_compression")

package(default_visibility = ["//visibility:public"])

config_setting(
    name = "engine_null",
    values = {"define": "engine=null"},
)

config_setting(
    name = "engine_v8",
    values = {"define": "engine=v8"},
)

config_setting(
    name = "engine_wamr_interp",
    values = {"define": "engine=wamr-interp"},
)

config_setting(
    name = "engine_wamr_jit",
    values = {"define": "engine=wamr-jit"},
)

config_setting(
    name = "engine_wasmedge",
    values = {"define": "engine=wasmedge"},
)

config_setting(
    name = "engine_wasmtime",
    values = {"define": "engine=wasmtime"},
)

config_setting(
    name = "multiengine",
    values = {"define": "engine=multi"},
)

config_setting(
    name = "crypto_system",
    values = {"define": "crypto=system"},
)

# V8 library with pointer compression disabled using V8's Starlark transition.
# This avoids the need to patch V8's default pointer compression setting.
v8_lib_no_pointer_compression(
    name = "wee8_no_pointer_compression",
    library = "@v8//:wee8",
)

exports_files(["tsan_suppressions.txt"])
