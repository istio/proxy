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
load(":x_tools_imports.bzl", "go_x_tools_imports_repositories")

GOOGLETEST = "d225acc90bc3a8c420a9bcd1f033033c1ccd7fe0"
GOOGLETEST_SHA256 = "01508c8f47c99509130f128924f07f3a60be05d039cff571bb11d60bb11a3581"

def googletest_repositories(bind = True):
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

#
# To update these...
# 1) find the ISTIO_API SHA you want in git
# 2) wget https://github.com/istio/api/archive/$ISTIO_API_SHA.tar.gz && sha256sum $ISTIO_API_SHA.tar.gz
#
ISTIO_API = "31d048906d97fb7f6b1fa8e250d3fa07456c5acc"
ISTIO_API_SHA256 = "5bf68ef13f4b9e769b7ca0a9ce83d9da5263eed9b1223c4cbb388a6ad5520e01"
GOGOPROTO_RELEASE = "1.2.1"
GOGOPROTO_SHA256 = "99e423905ba8921e86817607a5294ffeedb66fdd4a85efce5eb2848f715fdb3a"

def mixerapi_repositories(bind = True):
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

proto_library(
    name = "mixer_api_protos_lib",
    srcs = glob(
        [
            "mixer/v1/*.proto",
        ],
    ),
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_gogo_protobuf//:gogo_proto",
        "@com_google_googleapis//google/rpc:status_proto",
        "@com_google_protobuf//:duration_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "mixer_api_cc_proto",
    deps = [
        ":mixer_api_protos_lib",
    ],
    visibility = ["//visibility:public"],
)

proto_library(
    name = "mixer_client_config_proto_lib",
    srcs = glob(
        [
        "mixer/v1/config/client/*.proto",
        ],
    ),
    visibility = ["//visibility:public"],
    deps = [
        ":mixer_api_protos_lib",
        "@com_github_gogo_protobuf//:gogo_proto",
        "@com_google_googleapis//google/api:field_behavior_proto",
        "@com_google_protobuf//:duration_proto",
    ],
)

cc_proto_library(
    name = "mixer_client_config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":mixer_client_config_proto_lib",
    ],
)

proto_library(
    name = "authentication_policy_config_proto_lib",
    srcs = glob(
        ["envoy/config/filter/http/authn/v2alpha1/*.proto",
         "authentication/v1alpha1/*.proto",
         "common/v1alpha1/*.proto",
        ],
    ),
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_googleapis//google/api:field_behavior_proto",
        "@com_github_gogo_protobuf//:gogo_proto",
    ],
)

cc_proto_library(
    name = "authentication_policy_config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":authentication_policy_config_proto_lib",
    ],
)

proto_library(
    name = "jwt_auth_config_proto_lib",
    srcs = glob(
        ["envoy/config/filter/http/jwt_auth/v2alpha1/*.proto", ],
    ),
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_gogo_protobuf//:gogo_proto",
        "@com_google_protobuf//:duration_proto",
    ],
)

cc_proto_library(
    name = "jwt_auth_config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":jwt_auth_config_proto_lib",
    ],
)

proto_library(
    name = "alpn_filter_config_proto_lib",
    srcs = glob(
        ["envoy/config/filter/http/alpn/v2alpha1/*.proto", ],
    ),
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_gogo_protobuf//:gogo_proto",
    ],
)

cc_proto_library(
    name = "alpn_filter_config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":alpn_filter_config_proto_lib",
    ],
)

proto_library(
    name = "tcp_cluster_rewrite_config_proto_lib",
    srcs = glob(
        ["envoy/config/filter/network/tcp_cluster_rewrite/v2alpha1/*.proto", ],
    ),
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_gogo_protobuf//:gogo_proto",
    ],
)

cc_proto_library(
    name = "tcp_cluster_rewrite_config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":tcp_cluster_rewrite_config_proto_lib",
    ],
)

filegroup(
    name = "global_dictionary_file",
    srcs = ["mixer/v1/global_dictionary.yaml"],
    visibility = ["//visibility:public"],
)

"""
    GOGOPROTO_BUILD = """
load("@com_google_protobuf//:protobuf.bzl", "py_proto_library")
load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")

proto_library(
    name = "gogo_proto",
    srcs = ["gogoproto/gogo.proto"],
    deps = ["@com_google_protobuf//:descriptor_proto"],
    visibility = ["//visibility:public"],
)

go_proto_library(
    name = "descriptor_go_proto",
    importpath = "github.com/golang/protobuf/protoc-gen-go/descriptor",
    proto = "@com_google_protobuf//:descriptor_proto",
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "gogo_proto_cc",
    deps = [":gogo_proto"],
    visibility = ["//visibility:public"],
)

go_proto_library(
    name = "gogo_proto_go",
    importpath = "gogoproto",
    proto = ":gogo_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":descriptor_go_proto",
    ],
)

py_proto_library(
    name = "gogo_proto_py",
    srcs = [
        "gogoproto/gogo.proto",
    ],
    default_runtime = "@com_google_protobuf//:protobuf_python",
    protoc = "@com_google_protobuf//:protoc",
    visibility = ["//visibility:public"],
    deps = ["@com_google_protobuf//:protobuf_python"],
)
"""
    http_archive(
        name = "com_github_gogo_protobuf",
        build_file_content = GOGOPROTO_BUILD,
        strip_prefix = "protobuf-" + GOGOPROTO_RELEASE,
        url = "https://github.com/gogo/protobuf/archive/v" + GOGOPROTO_RELEASE + ".tar.gz",
        sha256 = GOGOPROTO_SHA256,
    )
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
            name = "alpn_filter_config_cc_proto",
            actual = "@mixerapi_git//:alpn_filter_config_cc_proto",
        )
        native.bind(
            name = "tcp_cluster_rewrite_config_cc_proto",
            actual = "@mixerapi_git//:tcp_cluster_rewrite_config_cc_proto",
        )

def mixerapi_dependencies():
    go_x_tools_imports_repositories()
    mixerapi_repositories()

def docker_dependencies():
    http_archive(
        name = "io_bazel_rules_docker",
        sha256 = "413bb1ec0895a8d3249a01edf24b82fd06af3c8633c9fb833a0cb1d4b234d46d",
        strip_prefix = "rules_docker-0.12.0",
        urls = ["https://github.com/bazelbuild/rules_docker/releases/download/v0.12.0/rules_docker-v0.12.0.tar.gz"],
    )
