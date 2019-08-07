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
    name = "metric",
    srcs = [
        "record.cc",
        "registry.cc",
    ],
    hdrs = [
        "record.h",
        "registry.h",
    ],
    visibility = [
        "//extensions/stackdriver:__pkg__",
    ],
    deps = [
        "//extensions/stackdriver/common:context",
        "//extensions/stackdriver/config/v1alpha1:stackdriver_plugin_config_cc_proto",
        "@io_opencensus_cpp//opencensus/exporters/stats/stackdriver:stackdriver_exporter",
        "@io_opencensus_cpp//opencensus/stats",
    ],
)

cc_test(
    name = "registry_test",
    size = "small",
    srcs = ["registry_test.cc"],
    linkstatic = 1,
    deps = [
        ":metric",
        "//external:googletest_main",
    ],
)
