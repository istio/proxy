# Copyright 2024 The Bazel Authors. All rights reserved.
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
"""ObjcInfo"""

load("@cc_compatibility_proxy//:symbols.bzl", _ObjcInfo = "ObjcInfo", _new_objc_provider = "new_objc_provider")

ObjcInfo = _ObjcInfo

# This is the same as ObjcInfo with Bazel 7 release.
# But it has to be a separate symbol to support Bazel 6.
new_objc_provider = _new_objc_provider
