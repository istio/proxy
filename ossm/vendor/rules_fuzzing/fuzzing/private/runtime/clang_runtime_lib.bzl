# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Macro that extracts clang runtime libraries from the current cc_toolchain."""

def clang_runtime_lib(*, name, basenames, **kwargs):
    """Provide the first available clang runtime library with any of the given basenames as output.

    The basename of the output file is always the first of the given basenames.
    """
    native.genrule(
        name = name,
        outs = basenames[:1],
        cmd = "\n".join(["""cp -f "$$($(CC) --print-file-name {})" $@ 2> /dev/null || true""".format(basename) for basename in basenames]),
        toolchains = ["@bazel_tools//tools/cpp:current_cc_toolchain"],
        tools = ["@bazel_tools//tools/cpp:current_cc_toolchain"],
        **kwargs
    )
