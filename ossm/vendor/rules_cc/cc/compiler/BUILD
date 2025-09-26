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

"""Config settings for compilers identified by Bazel.

Targets that require compiler-specific flags can use the config_settings defined
in this package in their select() statements.

*Note*: Before Bazel 6, gcc on Linux and clang on macOS would not match their
specific config_setting, but only the fallback case of a select expression.

Toolchains not shipped with Bazel are encouraged to use the same names to
identify compilers as used below, but this is not enforced.

Example:

  cc_binary(
    name = "foo",
    srcs = ["foo.cc"],
    copts = select({
        "//cc/compiler:gcc": [...],
        "//cc/compiler:clang": [...],
        "//cc/compiler:msvc-cl": [...],
        # Fallback case for an undetected compiler.
        "//conditions:default": [...],
    }),
  )

If multiple targets use the same set of conditionally enabled flags, this can be
simplified by extracting the select expression into a Starlark constant.
"""

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

config_setting(
    name = "clang",
    flag_values = {"@rules_cc//cc/private/toolchain:compiler": "clang"},
)

config_setting(
    name = "clang-cl",
    flag_values = {"@rules_cc//cc/private/toolchain:compiler": "clang-cl"},
)

config_setting(
    name = "gcc",
    flag_values = {"@rules_cc//cc/private/toolchain:compiler": "gcc"},
)

config_setting(
    name = "mingw-gcc",
    flag_values = {"@rules_cc//cc/private/toolchain:compiler": "mingw-gcc"},
)

config_setting(
    name = "msvc-cl",
    flag_values = {"@rules_cc//cc/private/toolchain:compiler": "msvc-cl"},
)

config_setting(
    name = "emscripten",
    flag_values = {"@rules_cc//cc/private/toolchain:compiler": "emscripten"},
)
