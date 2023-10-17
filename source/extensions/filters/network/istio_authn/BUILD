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
    "envoy_cc_extension",
    "envoy_cc_test",
    "envoy_extension_package",
    "envoy_proto_library",
)

envoy_extension_package()

envoy_cc_extension(
    name = "config_lib",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    deps = [
        ":config_cc_proto",
        "@envoy//envoy/common:hashable_interface",
        "@envoy//envoy/network:filter_interface",
        "@envoy//envoy/registry",
        "@envoy//envoy/server:filter_config_interface",
        "@envoy//envoy/stream_info:filter_state_interface",
        "@envoy//source/common/common:hash_lib",
        "@envoy//source/extensions/filters/network/common:factory_base_lib",
    ],
)

envoy_proto_library(
    name = "config",
    srcs = ["config.proto"],
)

envoy_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    repository = "@envoy",
    deps = [
        ":config_lib",
        "@envoy//source/common/buffer:buffer_lib",
        "@envoy//source/common/stream_info:filter_state_lib",
        "@envoy//test/mocks:common_lib",
        "@envoy//test/mocks/network:network_mocks",
    ],
)
