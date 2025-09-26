# Copyright 2015 The Bazel Authors. All rights reserved.
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

"""Module with Rust definitions required to write custom Rust rules."""

load(
    "//rust/private:common.bzl",
    _COMMON_PROVIDERS = "COMMON_PROVIDERS",
)
load(
    "//rust/private:providers.bzl",
    _BuildInfo = "BuildInfo",
    _ClippyInfo = "ClippyInfo",
    _CrateInfo = "CrateInfo",
    _DepInfo = "DepInfo",
    _DepVariantInfo = "DepVariantInfo",
    _TestCrateInfo = "TestCrateInfo",
)

BuildInfo = _BuildInfo
ClippyInfo = _ClippyInfo
CrateInfo = _CrateInfo
DepInfo = _DepInfo
DepVariantInfo = _DepVariantInfo
TestCrateInfo = _TestCrateInfo

COMMON_PROVIDERS = _COMMON_PROVIDERS
