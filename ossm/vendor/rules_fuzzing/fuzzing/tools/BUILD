# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This package contains tool scripts

load("@fuzzing_py_deps//:requirements.bzl", "requirement")
load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")
load("@rules_shell//shell:sh_binary.bzl", "sh_binary")

# Tools.
########

py_binary(
    name = "launcher",
    srcs = ["launcher.py"],
    python_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [requirement("absl-py")],
)

py_binary(
    name = "make_corpus_dir",
    srcs = ["make_corpus_dir.py"],
    python_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [requirement("absl-py")],
)

py_binary(
    name = "validate_dict",
    srcs = ["validate_dict.py"],
    python_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [
        ":dict_validation",
        requirement("absl-py"),
    ],
)

sh_binary(
    name = "noop_lcov_merger",
    srcs = ["noop_lcov_merger.sh"],
)

config_setting(
    name = "is_collecting_code_coverage",
    values = {"collect_code_coverage": "true"},
)

alias(
    name = "lcov_merger",
    actual = select({
        ":is_collecting_code_coverage": "@bazel_tools//tools/test:lcov_merger",
        "//conditions:default": ":noop_lcov_merger",
    }),
    visibility = ["//visibility:public"],
)

# Libraries.
############

py_library(
    name = "dict_validation",
    srcs = ["dict_validation.py"],
)

# Tests.
########

py_test(
    name = "dict_validation_test",
    srcs = ["dict_validation_test.py"],
    deps = [":dict_validation"],
)
