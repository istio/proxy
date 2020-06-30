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
workspace(name = "io_istio_proxy")

# http_archive is not a native function since bazel 0.19
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")
load(
    "//:repositories.bzl",
    "docker_dependencies",
    "googletest_repositories",
    "mixerapi_dependencies",
)

googletest_repositories()

mixerapi_dependencies()

bind(
    name = "boringssl_crypto",
    actual = "//external:ssl",
)

# 1. Determine SHA256 `wget https://github.com/envoyproxy/envoy-wasm/archive/$COMMIT.tar.gz && sha256sum $COMMIT.tar.gz`
# 2. Update .bazelversion, envoy.bazelrc and .bazelrc if needed.
#
# Commit time: 6/30/20
ENVOY_SHA = "06d236f62097bebc3af7b4f3154b7beca73f1339"

ENVOY_SHA256 = "7ecf350a7c26e64cfcdc5a3fba2db0b18f232a96ed65a4fc6cde4b6376cb0744"

ENVOY_ORG = "envoyproxy"

ENVOY_REPO = "envoy-wasm"

# To override with local envoy, just pass `--override_repository=envoy=/PATH/TO/ENVOY` to Bazel or
# persist the option in `user.bazelrc`.
http_archive(
    name = "envoy",
    sha256 = ENVOY_SHA256,
    strip_prefix = ENVOY_REPO + "-" + ENVOY_SHA,
    url = "https://github.com/" + ENVOY_ORG + "/" + ENVOY_REPO + "/archive/" + ENVOY_SHA + ".tar.gz",
)

load("@envoy//bazel:api_binding.bzl", "envoy_api_binding")

envoy_api_binding()

load("@envoy//bazel:api_repositories.bzl", "envoy_api_dependencies")

envoy_api_dependencies()

load("@envoy//bazel:repositories.bzl", "envoy_dependencies")

envoy_dependencies()

load("@envoy//bazel:repositories_extra.bzl", "envoy_dependencies_extra")

envoy_dependencies_extra()

load("@envoy//bazel:dependency_imports.bzl", "envoy_dependency_imports")

envoy_dependency_imports()

load("@rules_antlr//antlr:deps.bzl", "antlr_dependencies")

antlr_dependencies(471)

# Docker dependencies

docker_dependencies()

load(
    "@io_bazel_rules_docker//repositories:repositories.bzl",
    container_repositories = "repositories",
)

container_repositories()

load("@io_bazel_rules_docker//repositories:deps.bzl", container_deps = "deps")

container_deps()

load(
    "@io_bazel_rules_docker//container:container.bzl",
    "container_pull",
)

container_pull(
    name = "distroless_cc",
    # Latest as of 10/21/2019. To update, remove this line, re-build, and copy the suggested digest.
    digest = "sha256:86f16733f25964c40dcd34edf14339ddbb2287af2f7c9dfad88f0366723c00d7",
    registry = "gcr.io",
    repository = "distroless/cc",
)

container_pull(
    name = "bionic",
    # Latest as of 10/21/2019. To update, remove this line, re-build, and copy the suggested digest.
    digest = "sha256:3e83eca7870ee14a03b8026660e71ba761e6919b6982fb920d10254688a363d4",
    registry = "index.docker.io",
    repository = "library/ubuntu",
    tag = "bionic",
)

# End of docker dependencies

FLAT_BUFFERS_SHA = "a83caf5910644ba1c421c002ef68e42f21c15f9f"

http_archive(
    name = "com_github_google_flatbuffers",
    sha256 = "b8efbc25721e76780752bad775a97c3f77a0250271e2db37fc747b20e8b0f24a",
    strip_prefix = "flatbuffers-" + FLAT_BUFFERS_SHA,
    url = "https://github.com/google/flatbuffers/archive/" + FLAT_BUFFERS_SHA + ".tar.gz",
)

http_file(
    name = "com_github_nlohmann_json_single_header",
    sha256 = "3b5d2b8f8282b80557091514d8ab97e27f9574336c804ee666fda673a9b59926",
    urls = [
        "https://github.com/nlohmann/json/releases/download/v3.7.3/json.hpp",
    ],
)

COM_GOOGLE_ABSL_WASM_SHA = "768eb2ca2857342673fcd462792ce04b8bac3fa3"

http_archive(
    name = "com_google_absl_wasm",
    patch_args = ["-p1"],
    patches = [
        "@io_istio_proxy//:bazel/patches/absl.patch",
    ],
    sha256 = "bc9dd47d9676b016a8bec86f4e1cdc3edd22042bd9d7948a7b355f600974565e",
    strip_prefix = "abseil-cpp-" + COM_GOOGLE_ABSL_WASM_SHA,
    url = "https://github.com/abseil/abseil-cpp/archive/" + COM_GOOGLE_ABSL_WASM_SHA + ".tar.gz",
)
