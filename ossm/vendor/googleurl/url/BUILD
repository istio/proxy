# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
load("//build_config:build_config.bzl", "build_config")
load("@rules_cc//cc:defs.bzl", "cc_library")

idna_src = select({
    "//build_config:with_system_icu": ["url_idna_icu.cc"],
    "//conditions:default": ["url_idna_ascii_only.cc"],
})

cc_library(
    name = "url",
    srcs = [
        "gurl.cc",
        "third_party/mozilla/url_parse.cc",
        "url_canon.cc",
        "url_canon_etc.cc",
        "url_canon_filesystemurl.cc",
        "url_canon_fileurl.cc",
        "url_canon_host.cc",
        "url_canon_internal.cc",
        "url_canon_internal.h",
        "url_canon_internal_file.h",
        "url_canon_ip.cc",
        "url_canon_mailtourl.cc",
        "url_canon_path.cc",
        "url_canon_pathurl.cc",
        "url_canon_query.cc",
        "url_canon_relative.cc",
        "url_canon_stdstring.cc",
        "url_canon_stdurl.cc",
        "url_constants.cc",
        "url_parse_file.cc",
        "url_parse_internal.h",
        "url_util.cc",
        "url_util_internal.h",
    ] + idna_src,
    hdrs = [
        "gurl.h",
        "third_party/mozilla/url_parse.h",
        "url_canon.h",
        "url_canon_icu.h",
        "url_canon_ip.h",
        "url_canon_stdstring.h",
        "url_constants.h",
        "url_file.h",
        "url_util.h",
    ],
    copts = build_config.default_copts,
    linkopts = build_config.url_linkopts,
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//polyfills",
    ],
)
