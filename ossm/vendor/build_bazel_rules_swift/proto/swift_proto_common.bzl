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

"""A resilient API layer wrapping Swift proto compilation.

This module is meant to be used by custom rules that need to compile Swift protos
and cannot simply rely on writing a macro that wraps `swift_proto_library` 
or define a custom `swift_proto_compiler` target.
"""

load(
    "//proto:swift_proto_utils.bzl",
    "compile_swift_protos_for_target",
    "proto_path",
)

swift_proto_common = struct(
    proto_path = proto_path,
    compile_swift_protos_for_target = compile_swift_protos_for_target,
)
