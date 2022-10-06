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
load("@rules_pkg//:pkg.bzl", "pkg_tar")
load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_binary",
)

envoy_cc_binary(
    name = "envoy",
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        "//extensions/access_log_policy:access_log_policy_lib",
        "//extensions/attributegen:attributegen_plugin",
        "//extensions/metadata_exchange:metadata_exchange_lib",
        "//extensions/stackdriver:stackdriver_plugin",
        "//extensions/stats:stats_plugin",
        "//source/extensions/filters/http/alpn:config_lib",
        "//source/extensions/filters/http/authn:filter_lib",
        "//source/extensions/filters/http/istio_stats",
        "//source/extensions/filters/network/forward_downstream_sni:config_lib",
        "//source/extensions/filters/network/metadata_exchange:config_lib",
        "//source/extensions/filters/network/sni_verifier:config_lib",
        "//source/extensions/filters/network/tcp_cluster_rewrite:config_lib",
        "//src/envoy/http/baggage_handler:config_lib",  # Experimental: Ambient
        "//src/envoy/metadata_to_peer_node:config_lib",  # Experimental: Ambient
        "//src/envoy/set_internal_dst_address:filter_lib",  # Experimental: Ambient
        "//src/envoy/tls_passthrough:filter_lib",  # Experimental: Ambient
        "//src/envoy/workload_metadata:config_lib",  # Experimental: Ambient
        "@envoy//source/exe:envoy_main_entry_lib",
    ],
)

pkg_tar(
    name = "envoy_tar",
    srcs = [":envoy"],
    extension = "tar.gz",
    mode = "0755",
    package_dir = "/usr/local/bin/",
    tags = ["manual"],
    visibility = ["//visibility:public"],
)
