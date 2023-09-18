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

#
# To update these...
# 1) find the ISTIO_API SHA you want in git
# 2) wget https://github.com/istio/api/archive/$ISTIO_API_SHA.tar.gz && sha256sum $ISTIO_API_SHA.tar.gz
#
ISTIO_API = "75bb24b620144218d26b92afedbb428e4d84e506"
ISTIO_API_SHA256 = "c72602c38f7ab10e430618e4ce82fee143f4446d468863ba153ea897bdff2298"

def istioapi_repositories(bind = True):
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
    name = "alpn_filter_config_proto_lib",
    srcs = glob(
        ["envoy/config/filter/http/alpn/v2alpha1/*.proto", ],
    ),
    visibility = ["//visibility:public"],
)

cc_proto_library(
    name = "alpn_filter_config_cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":alpn_filter_config_proto_lib",
    ],
)
"""
    http_archive(
        name = "istioapi_git",
        build_file_content = BUILD,
        strip_prefix = "api-" + ISTIO_API,
        url = "https://github.com/istio/api/archive/" + ISTIO_API + ".tar.gz",
        sha256 = ISTIO_API_SHA256,
    )
    if bind:
        native.bind(
            name = "authentication_policy_config_cc_proto",
            actual = "@istioapi_git//:authentication_policy_config_cc_proto",
        )
        native.bind(
            name = "alpn_filter_config_cc_proto",
            actual = "@istioapi_git//:alpn_filter_config_cc_proto",
        )

def istioapi_dependencies():
    istioapi_repositories()
