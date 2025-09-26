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
# -*- coding: utf-8 -*-

load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

licenses(["notice"])

pkg_tar(
    name = "never_stamped",
    srcs = [
        ":BUILD",
    ],
)

pkg_tar(
    name = "always_stamped",
    srcs = [
        ":BUILD",
    ],
    stamp = 1,
)

pkg_tar(
    name = "controlled_by_stamp_option",
    srcs = [
        ":BUILD",
    ],
    stamp = -1,
)
