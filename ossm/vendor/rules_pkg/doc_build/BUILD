# Copyright 2021 The Bazel Authors. All rights reserved.
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
"""Generate the reference documentation.

How to:
   bazel build //doc_build:reference
   cp bazel-bin/doc_build/reference.md docs/latest.md
   git commit -m 'update docs' docs/latest.md
"""

load("@bazel_skylib//:bzl_library.bzl", "bzl_library")
load("@rules_python//python:defs.bzl", "py_binary")
load("@stardoc//stardoc:stardoc.bzl", "stardoc")
load("//:version.bzl", "version")

package(default_applicable_licenses = ["//:license"])

filegroup(
    name = "standard_package",
    srcs = [
        "BUILD",
    ] + glob([
        "*.py",
    ]),
    visibility = ["//distro:__pkg__"],
)

# pairs of rule name and the source file to get it from
# buildifier: leave-alone, do not sort
ORDER = [
    ("toc", None),
    ("common", None),
    ("pkg_deb", "//pkg/private/deb:deb.bzl"),
    ("pkg_deb_impl", "//pkg/private/deb:deb.bzl"),
    ("pkg_sub_rpm", "//pkg:rpm_pfg.bzl"),
    ("pkg_rpm", "//pkg:rpm_pfg.bzl"),
    ("pkg_tar", "//pkg/private/tar:tar.bzl"),
    ("pkg_tar_impl", "//pkg/private/tar:tar.bzl"),
    ("pkg_zip", "//pkg/private/zip:zip.bzl"),
    ("pkg_zip_impl", "//pkg/private/zip:zip.bzl"),
    ("mappings", None),
    ("legacy_pkg_rpm", None),
]

genrule(
    name = "reference",
    srcs = ["%s.md" % rule for rule, _ in ORDER],
    outs = ["reference.md"],
    cmd = "$(location :merge) $(SRCS) >$@",
    tools = [":merge"],
)

[
    stardoc(
        name = "%s_gen" % rule,
        out = "%s.md" % rule,
        input = src,
        symbol_names = [
            rule,
        ],
        deps = [
            ":rules_pkg_lib",
            "//toolchains/rpm:standard_package",
        ],
    )
    for rule, src in ORDER
    if src
]

genrule(
    name = "toc",
    srcs = ["toc.md.tpl"],
    outs = ["toc.md"],
    cmd = "sed -e 's/{VERSION}/%s/' $(SRCS) >$@" % version,
)

# Generate separately or there will be a conflict with the other pkg_rpm.
stardoc(
    name = "docs_legacy_rpm",
    out = "legacy_pkg_rpm.md",
    input = "//pkg/legacy:rpm.bzl",
    deps = [":rules_pkg_lib"],
)

# Mappings has a lot of pure rules, so it is mostly in a good order.
stardoc(
    name = "mappings",
    out = "mappings.md",
    input = "//pkg:mappings.bzl",
    deps = [
        ":rules_pkg_lib",
    ],
)

# gather all rules that should be documented
bzl_library(
    name = "rules_pkg_lib",
    srcs = [
        "//:version.bzl",
        "//pkg:bzl_srcs",
        "@bazel_skylib//lib:paths",
    ],
    visibility = ["//visibility:public"],
)

py_binary(
    name = "merge",
    srcs = ["merge.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    visibility = ["//visibility:private"],
)
