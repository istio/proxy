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
load("@io_bazel_rules_docker//container:image.bzl", "container_image")
load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_push",
)

container_image(
    name = "envoy_distroless",
    base = "@distroless_cc//image",
    tags = ["manual"],
    tars = ["//src/envoy:envoy_tar"],
)

container_image(
    name = "envoy_ubuntu",
    base = "@bionic//image",
    tags = ["manual"],
    tars = ["//src/envoy:envoy_tar"],
)

container_push(
    name = "push_envoy_distroless",
    format = "Docker",
    image = ":envoy_distroless",
    registry = "gcr.io",
    repository = "{DOCKER_REPOSITORY}",
    tag = "{BUILD_CONFIG}-{BUILD_SCM_REVISION}",
)

container_push(
    name = "push_envoy_ubuntu",
    format = "Docker",
    image = ":envoy_ubuntu",
    registry = "gcr.io",
    repository = "{DOCKER_REPOSITORY}",
    tag = "ubuntu-{BUILD_CONFIG}-{BUILD_SCM_REVISION}",
)
