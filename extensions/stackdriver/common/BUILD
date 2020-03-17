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

licenses(["notice"])

cc_library(
    name = "constants",
    hdrs = [
        "constants.h",
    ],
    visibility = [
        "//extensions/stackdriver:__pkg__",
        "//extensions/stackdriver/edges:__pkg__",
        "//extensions/stackdriver/log:__pkg__",
        "//extensions/stackdriver/metric:__pkg__",
    ],
)

cc_library(
    name = "utils",
    srcs = [
        "utils.cc",
    ],
    hdrs = [
        "utils.h",
    ],
    visibility = [
        "//extensions/stackdriver:__pkg__",
        "//extensions/stackdriver/log:__pkg__",
        "//extensions/stackdriver/metric:__pkg__",
    ],
    deps = [
        ":constants",
        "//extensions/common:context",
        "@com_google_googleapis//google/monitoring/v3:monitoring_cc_proto",
    ],
)
