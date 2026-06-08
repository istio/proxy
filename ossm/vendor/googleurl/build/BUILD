# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
load("//build_config:build_config.bzl", "build_config")
load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "build_config",
    hdrs = ["build_config.h"],
    copts = build_config.default_copts,
    visibility = ["//visibility:public"],
)

cc_library(
    name = "buildflag",
    hdrs = ["buildflag.h"],
    copts = build_config.default_copts,
    visibility = ["//visibility:public"],
)
