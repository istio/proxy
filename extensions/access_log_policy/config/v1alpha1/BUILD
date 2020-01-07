# Copyright 2019 Istio Authors. All Rights Reserved.
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

cc_proto_library(
    name = "access_log_policy_config_cc_proto",
    visibility = [
        "//extensions/access_log_policy:__pkg__",
    ],
    deps = ["access_log_policy_config_proto"],
)

proto_library(
    name = "access_log_policy_config_proto",
    srcs = ["access_log_policy_config.proto"],
    deps = [
        "@com_google_protobuf//:duration_proto",
    ],
)
