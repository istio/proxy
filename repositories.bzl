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

GOOGLETEST = "d225acc90bc3a8c420a9bcd1f033033c1ccd7fe0"
GOOGLETEST_SHA256 = "01508c8f47c99509130f128924f07f3a60be05d039cff571bb11d60bb11a3581"

def googletest_repositories(bind=True):
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
cc_library(
    name = "googletest",
    srcs = [
        "googletest/src/gtest-all.cc",
        "googlemock/src/gmock-all.cc",
    ],
    hdrs = glob([
        "googletest/include/**/*.h",
        "googlemock/include/**/*.h",
        "googletest/src/*.cc",
        "googletest/src/*.h",
        "googlemock/src/*.cc",
    ]),
    includes = [
        "googlemock",
        "googletest",
        "googletest/include",
        "googlemock/include",
    ],
    visibility = ["//visibility:public"],
)
cc_library(
    name = "googletest_main",
    srcs = ["googlemock/src/gmock_main.cc"],
    visibility = ["//visibility:public"],
    deps = [":googletest"],
)
cc_library(
    name = "googletest_prod",
    hdrs = [
        "googletest/include/gtest/gtest_prod.h",
    ],
    includes = [
        "googletest/include",
    ],
    visibility = ["//visibility:public"],
)
"""
    http_archive(
        name = "googletest_git",
        build_file_content = BUILD,
        strip_prefix = "googletest-" + GOOGLETEST,
        url = "https://github.com/google/googletest/archive/" + GOOGLETEST + ".tar.gz",
        sha256 = GOOGLETEST_SHA256,
    )

    if bind:
        native.bind(
            name = "googletest",
            actual = "@googletest_git//:googletest",
        )

        native.bind(
            name = "googletest_main",
            actual = "@googletest_git//:googletest_main",
        )

        native.bind(
            name = "googletest_prod",
            actual = "@googletest_git//:googletest_prod",
        )

ISTIO_API = "6b9e3a501e6ef254958bf82f7b74c37d64a57a15"
ISTIO_API_SHA256 = "ce43fcc51bd7c653d39b810e50df68e32ed95991919c1d1f2f56b331d79e674e"

def mixerapi_repositories(bind=True):
    BUILD = """
# Copyright 2018 Istio Authors. All Rights Reserved.
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
load("@com_google_protobuf//:protobuf.bzl", "cc_proto_library")

cc_proto_library(
    name = "mixer_api_cc_proto",
    srcs = glob(
        ["mixer/v1/*.proto"],
    ),
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    visibility = ["//visibility:public"],
    deps = [
        "//external:cc_gogoproto",
        "//external:cc_wkt_protos",
        "//external:rpc_status_proto",
    ],
)

cc_proto_library(
    name = "mixer_client_config_cc_proto",
    srcs = glob(
        ["mixer/v1/config/client/*.proto"],
    ),
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    visibility = ["//visibility:public"],
    deps = [
        ":mixer_api_cc_proto",
    ],
)

cc_proto_library(
    name = "authentication_policy_config_cc_proto",
    srcs = glob(
        ["envoy/config/filter/http/authn/v2alpha1/*.proto",
         "authentication/v1alpha1/*.proto",
         "common/v1alpha1/*.proto",
        ],
    ),
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    visibility = ["//visibility:public"],
    deps = [
        "//external:cc_gogoproto",
    ],
)

cc_proto_library(
    name = "jwt_auth_config_cc_proto",
    srcs = glob(
        ["envoy/config/filter/http/jwt_auth/v2alpha1/*.proto", ],
    ),
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    visibility = ["//visibility:public"],
    deps = [
        "//external:cc_gogoproto",
    ],
)

cc_proto_library(
    name = "tcp_cluster_rewrite_config_cc_proto",
    srcs = glob(
        ["envoy/config/filter/network/tcp_cluster_rewrite/v2alpha1/*.proto", ],
    ),
    default_runtime = "//external:protobuf",
    protoc = "//external:protoc",
    visibility = ["//visibility:public"],
    deps = [
        "//external:cc_gogoproto",
    ],
)

filegroup(
    name = "global_dictionary_file",
    srcs = ["mixer/v1/global_dictionary.yaml"],
    visibility = ["//visibility:public"],
)

"""
    http_archive(
        name = "mixerapi_git",
        build_file_content = BUILD,
        strip_prefix = "api-" + ISTIO_API,
        url = "https://github.com/istio/api/archive/" + ISTIO_API + ".tar.gz",
        sha256 = ISTIO_API_SHA256,
    )
    if bind:
        native.bind(
            name = "mixer_api_cc_proto",
            actual = "@mixerapi_git//:mixer_api_cc_proto",
        )
        native.bind(
            name = "mixer_client_config_cc_proto",
            actual = "@mixerapi_git//:mixer_client_config_cc_proto",
        )
        native.bind(
            name = "authentication_policy_config_cc_proto",
            actual = "@mixerapi_git//:authentication_policy_config_cc_proto",
        )
        native.bind(
            name = "jwt_auth_config_cc_proto",
            actual = "@mixerapi_git//:jwt_auth_config_cc_proto",
        )
        native.bind(
            name = "tcp_cluster_rewrite_config_cc_proto",
            actual = "@mixerapi_git//:tcp_cluster_rewrite_config_cc_proto",
        )

load(":protobuf.bzl", "protobuf_repositories")
load(":cc_gogo_protobuf.bzl", "cc_gogoproto_repositories")
load(":x_tools_imports.bzl", "go_x_tools_imports_repositories")
load(":googleapis.bzl", "googleapis_repositories")

def  mixerapi_dependencies():
     protobuf_repositories(load_repo=True, bind=True)
     cc_gogoproto_repositories()
     go_x_tools_imports_repositories()
     googleapis_repositories()
     mixerapi_repositories()
