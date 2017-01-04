# Copyright 2016 Google Inc. All Rights Reserved.
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
    "//:repositories.bzl",
    "boringssl_repositories",
    "protobuf_repositories",
    "googletest_repositories",
)

boringssl_repositories()

protobuf_repositories()

googletest_repositories()

load(
    "//contrib/endpoints:repositories.bzl",
    "grpc_repositories",
    "mixerapi_repositories",
    "servicecontrol_client_repositories",
)

grpc_repositories()

mixerapi_repositories()

servicecontrol_client_repositories()

load(
    "//src/envoy:repositories.bzl",
    "envoy_repositories",
)

envoy_repositories()
