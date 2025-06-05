# Copyright 2024 The Bazel Authors. All rights reserved.
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

load("@bazel_skylib//rules:diff_test.bzl", "diff_test")
load("@bazel_skylib//rules:expand_template.bzl", "expand_template")
load("@stardoc//stardoc:stardoc.bzl", "stardoc")
load("//cc/toolchains/impl:documented_api.bzl", "DOCUMENTED_TOOLCHAIN_RULES")
load("//cc/toolchains/impl:markdown_helpers.bzl", "xref_substitutions")

filegroup(
    name = "srcs",
    srcs = glob(
        [
            "**/*.bzl",
            "**/BUILD",
        ],
        allow_empty = True,
    ),
    visibility = ["//visibility:public"],
)

stardoc(
    name = "toolchain_api",
    out = "raw_generated_toolchain_api.md",
    input = "//cc/toolchains/impl:documented_api.bzl",
    deps = ["//cc/toolchains:toolchain_rules"],
)

expand_template(
    name = "toolchain_api_md",
    out = "generated_toolchain_api.md",
    # Dictionary order 100% matters here!
    # buildifier: disable=unsorted-dict-items
    substitutions = {
        # Strip @rules_cc to prevent instances of @rules_cc@rules_cc//cc.
        "@rules_cc//cc": "//cc",
        # In GitHub, we prefer to clarify all the labels that come from
        # rules_cc.
        "//cc": "@rules_cc//cc",
    } | xref_substitutions({
        "`{}`".format(rule_name): "#{}".format(rule_name)
        for rule_name in DOCUMENTED_TOOLCHAIN_RULES
    }),
    # buildifier: enable=unsorted-dict-items
    template = ":raw_generated_toolchain_api.md",
)

diff_test(
    name = "toolchain_api_diff_test",
    file1 = ":generated_toolchain_api.md",
    file2 = ":toolchain_api.md",
)
