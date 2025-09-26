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
"""Core rules for building Python projects."""

load("//python:py_binary.bzl", _py_binary = "py_binary")
load("//python:py_info.bzl", _PyInfo = "PyInfo")
load("//python:py_library.bzl", _py_library = "py_library")
load("//python:py_runtime.bzl", _py_runtime = "py_runtime")
load("//python:py_runtime_info.bzl", internal_PyRuntimeInfo = "PyRuntimeInfo")
load("//python:py_runtime_pair.bzl", _py_runtime_pair = "py_runtime_pair")
load("//python:py_test.bzl", _py_test = "py_test")
load(":current_py_toolchain.bzl", _current_py_toolchain = "current_py_toolchain")
load(":py_import.bzl", _py_import = "py_import")

# Patching placeholder: end of loads

PyInfo = _PyInfo

PyRuntimeInfo = internal_PyRuntimeInfo

current_py_toolchain = _current_py_toolchain

py_import = _py_import

py_runtime_pair = _py_runtime_pair

py_library = _py_library

py_binary = _py_binary

py_test = _py_test

py_runtime = _py_runtime
