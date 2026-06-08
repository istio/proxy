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

"""BUILD rules to define Swift proto libraries and compilers.

**NOTE:** This file is deprecated. To avoid having Bazel do more work than
necessary, users should import each rule/build definition they use from the
`.bzl` file that defines it in this directory.

Do not import any definitions directly from the `internal` directory; those are
meant for build rule use only.
"""

load(
    "//proto:swift_proto_common.bzl",
    _swift_proto_common = "swift_proto_common",
)
load(
    "//proto:swift_proto_compiler.bzl",
    _swift_proto_compiler = "swift_proto_compiler",
)
load(
    "//proto:swift_proto_library.bzl",
    _swift_proto_library = "swift_proto_library",
)
load(
    "//proto:swift_proto_library_group.bzl",
    _swift_proto_library_group = "swift_proto_library_group",
)
load(
    "//swift:providers.bzl",
    _SwiftProtoCompilerInfo = "SwiftProtoCompilerInfo",
    _SwiftProtoInfo = "SwiftProtoInfo",
)

# Export providers:
SwiftProtoCompilerInfo = _SwiftProtoCompilerInfo
SwiftProtoInfo = _SwiftProtoInfo

# Export rules:
swift_proto_compiler = _swift_proto_compiler
swift_proto_library = _swift_proto_library
swift_proto_library_group = _swift_proto_library_group

# Export modules:
swift_proto_common = _swift_proto_common
