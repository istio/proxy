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

load("@rules_shell//shell:sh_test.bzl", "sh_test")

sh_test(
    name = "system_library_test",
    size = "small",
    srcs = ["system_library_test.sh"],
    data = [
        ":unittest.bash",
        "//cc:system_library.bzl",
        "@bazel_tools//tools/bash/runfiles",
    ],
    target_compatible_with = [
        "@platforms//os:linux",
    ],
)
