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

TELEMETRY_GOOGLEAPIS_SHA="c39b7e880e6db2ce61704da2a55083ea17fdb14b"
TELEMETRY_GOOGLEAPIS_SHA256="ee05b85961aa721671d85c111c6287e9667e69b616d97959588b1a991ef44a2d"
TELEMETRY_GOOGLEAPIS_URLS=["https://github.com/googleapis/googleapis/archive/" + TELEMETRY_GOOGLEAPIS_SHA + ".tar.gz"]

def telemetry_googleapis():
    http_archive(
        name = "telemetry_googleapis",
        urls = TELEMETRY_GOOGLEAPIS_URLS,
        sha256 = TELEMETRY_GOOGLEAPIS_SHA256,
        strip_prefix = "googleapis-" + TELEMETRY_GOOGLEAPIS_SHA,
    )
