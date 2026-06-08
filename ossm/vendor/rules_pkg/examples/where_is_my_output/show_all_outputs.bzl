# -*- coding: utf-8 -*-
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
"""
Extract the paths to the various outputs of pkg_deb

Usage:
  bazel cquery //pkg:deb --output=starlark --starlark:file=show_all_outputs.bzl
"""

# buildifier: disable=function-docstring
def format(target):
    provider_map = providers(target)
    output_group_info = provider_map["OutputGroupInfo"]

    # Look at the attributes of the provider. Visit the depsets.
    ret = []
    for attr in dir(output_group_info):
        if attr.startswith("_"):
            continue
        attr_value = getattr(output_group_info, attr)
        if type(attr_value) == "depset":
            for file in attr_value.to_list():
                ret.append("%s: %s" % (attr, file.path))
    return "\n".join(ret)
