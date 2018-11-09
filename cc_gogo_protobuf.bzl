# Copyright 2017 Istio Authors. All Rights Reserved.
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
#
################################################################################
#
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

GOGO_PROTO_SHA = "100ba4e885062801d56799d78530b73b178a78f3"
GOGO_PROTO_SHA256 = "b04eb8eddd2d15d8b12d111d4ef7816fca6e5c5d495adf45fb8478278aa80f79"

def cc_gogoproto_repositories(bind=True):
    BUILD = """
# Copyright 2017 Istio Authors. All Rights Reserved.
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
#
################################################################################
#

licenses(["notice"])

load("@com_google_protobuf//:protobuf.bzl", "cc_proto_library")

exports_files(glob(["google/**"]))

cc_proto_library(
    name = "cc_gogoproto",
    srcs = [
        "gogoproto/gogo.proto",
    ],
    include = ".",
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    visibility = ["//visibility:public"],
    deps = [
        "//external:cc_wkt_protos",
    ],
)
"""
    http_archive(
        name = "gogoproto_git",
        strip_prefix = "protobuf-" + GOGO_PROTO_SHA,
        url = "https://github.com/gogo/protobuf/archive/" + GOGO_PROTO_SHA + ".tar.gz",
        sha256 = GOGO_PROTO_SHA256,
        build_file_content = BUILD,
    )

    if bind:
        native.bind(
            name = "cc_gogoproto",
            actual = "@gogoproto_git//:cc_gogoproto",
        )

        native.bind(
            name = "cc_gogoproto_genproto",
            actual = "@gogoproto_git//:cc_gogoproto_genproto",
        )
