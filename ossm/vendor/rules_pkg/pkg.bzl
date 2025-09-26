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

"""Rules for manipulation of various packaging."""

load("//pkg:deb.bzl", _pkg_deb = "pkg_deb")
load("//pkg:tar.bzl", _pkg_tar = "pkg_tar")
load("//pkg:zip.bzl", _pkg_zip = "pkg_zip")

pkg_deb = _pkg_deb
pkg_tar = _pkg_tar
pkg_zip = _pkg_zip
