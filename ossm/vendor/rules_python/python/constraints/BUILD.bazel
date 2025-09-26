# Copyright 2019 The Bazel Authors. All rights reserved.
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

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

filegroup(
    name = "distribution",
    srcs = glob(["**"]),
    visibility = ["//python:__pkg__"],
)

# A constraint_setting to use for constraints related to the location of the
# system Python 2 interpreter on a platform.
alias(
    name = "py2_interpreter_path",
    actual = "@bazel_tools//tools/python:py2_interpreter_path",
)

# A constraint_setting to use for constraints related to the location of the
# system Python 3 interpreter on a platform.
alias(
    name = "py3_interpreter_path",
    actual = "@bazel_tools//tools/python:py3_interpreter_path",
)
