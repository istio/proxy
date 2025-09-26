# Copyright 2022 The Bazel Authors. All rights reserved.
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
"""Providers for Python rules."""

load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load(":util.bzl", "define_bazel_6_provider")

def _PyCcLinkParamsInfo_init(cc_info):
    return {
        "cc_info": CcInfo(linking_context = cc_info.linking_context),
    }

# buildifier: disable=name-conventions
PyCcLinkParamsInfo, _unused_raw_py_cc_link_params_provider_ctor = define_bazel_6_provider(
    doc = ("Python-wrapper to forward {obj}`CcInfo.linking_context`. This is to " +
           "allow Python targets to propagate C++ linking information, but " +
           "without the Python target appearing to be a valid C++ rule dependency"),
    init = _PyCcLinkParamsInfo_init,
    fields = {
        "cc_info": """
:type: CcInfo

Linking information; it has only {obj}`CcInfo.linking_context` set.
""",
    },
)
