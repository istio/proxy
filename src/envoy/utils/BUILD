# Copyright 2016 Istio Authors. All Rights Reserved.
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
package(default_visibility = ["//visibility:public"])

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
)

envoy_cc_library(
    name = "jwt_lib",
    srcs = ["jwt.cc"],
    hdrs = ["jwt.h"],
    external_deps = [
        "rapidjson",
        "ssl",
    ],
    repository = "@envoy",
    deps = [
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "jwt_authenticator_lib",
    srcs = [
        "jwt_authenticator.cc",
        "token_extractor.cc",
    ],
    hdrs = [
        "auth_store.h",
        "jwt_authenticator.h",
        "pubkey_cache.h",
        "token_extractor.h",
    ],
    repository = "@envoy",
    deps = [
        ":jwt_lib",
        "@envoy_api//envoy/config/filter/http/jwt_authn/v2alpha:jwt_authn_cc",
        "@envoy_api//envoy/config/filter/http/common/v1alpha:common_cc",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "authn_lib",
    srcs = [
        "authn.cc",
    ],
    hdrs = [
        "authn.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//src/istio/authn:context_proto",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_library(
    name = "utils_lib",
    srcs = [
        "config.cc",
        "constants.cc",
        "grpc_transport.cc",
        "mixer_control.cc",
        "stats.cc",
        "utils.cc",
    ],
    hdrs = [
        "config.h",
        "constants.h",
        "grpc_transport.h",
        "mixer_control.h",
        "stats.h",
        "utils.h",
    ],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//external:mixer_client_config_cc_proto",
        "//src/istio/mixerclient:mixerclient_lib",
        "@envoy//source/exe:envoy_common_lib",
    ],
)

envoy_cc_test(
    name = "jwt_test",
    srcs = [
        "jwt_test.cc",
    ],
    data = [],
    repository = "@envoy",
    deps = [
        ":jwt_lib",
        "@envoy//source/exe:envoy_common_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "jwt_authenticator_test",
    srcs = [
        "jwt_authenticator_test.cc",
    ],
    data = [],
    repository = "@envoy",
    deps = [
        ":jwt_authenticator_lib",
        "@envoy//source/exe:envoy_common_lib",
        "@envoy//test/mocks/upstream:upstream_mocks",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "token_extractor_test",
    srcs = [
        "token_extractor_test.cc",
    ],
    data = [],
    repository = "@envoy",
    deps = [
        ":jwt_authenticator_lib",
        "@envoy//source/exe:envoy_common_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "authn_test",
    srcs = [
        "authn_test.cc",
    ],
    repository = "@envoy",
    deps = [
        ":authn_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "utils_test",
    srcs = [
        "utils_test.cc",
    ],
    repository = "@envoy",
    deps = [
        ":utils_lib",
        "@envoy//test/test_common:utility_lib",
    ],
)
