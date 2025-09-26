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

"""File mappings for packaging rules."""

load(
    "//pkg:mappings.bzl",
    _REMOVE_BASE_DIRECTORY = "REMOVE_BASE_DIRECTORY",
    _filter_directory = "filter_directory",
    _pkg_attributes = "pkg_attributes",
    _pkg_filegroup = "pkg_filegroup",
    _pkg_files = "pkg_files",
    _pkg_mkdirs = "pkg_mkdirs",
    _pkg_mklink = "pkg_mklink",
    _strip_prefix = "strip_prefix",
)

REMOVE_BASE_DIRECTORY = _REMOVE_BASE_DIRECTORY

filter_directory = _filter_directory

pkg_attributes = _pkg_attributes

pkg_filegroup = _pkg_filegroup

pkg_files = _pkg_files

pkg_mkdirs = _pkg_mkdirs

pkg_mklink = _pkg_mklink

strip_prefix = _strip_prefix
