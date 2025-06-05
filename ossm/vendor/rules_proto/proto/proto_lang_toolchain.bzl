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
"""proto_lang_toolchain rule"""

load("@com_google_protobuf//bazel/toolchains:proto_lang_toolchain.bzl", _proto_lang_toolchain = "proto_lang_toolchain")

proto_lang_toolchain = _proto_lang_toolchain
