# Copyright Istio Authors. All Rights Reserved.
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

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
)

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

envoy_cc_library(
    name = "hashable_string",
    srcs = ["hashable_string.cc"],
    hdrs = ["hashable_string.h"],
    repository = "@envoy",
    deps = [
        "@envoy//envoy/common:hashable_interface",
        "@envoy//envoy/registry",
        "@envoy//envoy/stream_info:filter_state_interface",
        "@envoy//source/common/common:hash_lib",
        "@envoy//source/common/router:string_accessor_lib",
    ],
)

envoy_cc_test(
    name = "hashable_string_test",
    srcs = ["hashable_string_test.cc"],
    repository = "@envoy",
    deps = [
        ":hashable_string",
        "@com_google_protobuf//:protobuf",
        "@envoy//envoy/common:hashable_interface",
        "@envoy//source/extensions/filters/common/set_filter_state:filter_config_lib",
        "@envoy//test/mocks/server:server_factory_context_mocks",
        "@envoy//test/mocks/stream_info:stream_info_mocks",
        "@envoy//test/test_common:utility_lib",
        "@envoy_api//envoy/extensions/filters/common/set_filter_state/v3:pkg_cc_proto",
    ],
)
