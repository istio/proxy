# Copyright 2016 Istio Authors. All Rights Reserved.
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

exports_files(["LICENSE"])

config_setting(
    name = "darwin",
    values = {
        "cpu": "darwin",
    },
    visibility = ["//visibility:public"],
)

load("@io_bazel_rules_go//go:def.bzl", "go_prefix")

go_prefix("istio.io/proxy")

load("@bazel_tools//tools/build_defs/pkg:pkg.bzl", "pkg_tar", "pkg_deb")

pkg_tar(
    name = "istio-bin",
    files = [
        "@pilot//cmd/pilot-agent:pilot-agent",
        "@pilot//cmd/pilot-discovery:pilot-discovery",
        "@pilot//docker:prepare_proxy",
        "@proxy//src/envoy/mixer:envoy",
    ],
    mode = "0755",
    package_dir = "/usr/local/bin",
)

pkg_tar(
    name = "istio-systemd",
    files = ["tools/deb/istio.service"],
    mode = "644",
    package_dir = "/lib/systemd/system",
)

pkg_tar(
    name = "debian-data",
    extension = "tar.gz",
    deps = [
        ":istio-bin",
        ":istio-systemd",
    ],
)

pkg_deb(
    name = "istio-proxy.deb",
    architecture = "amd64",
    built_using = "bazel",
    data = ":debian-data",
    depends = [
        "uuid-runtime",  # Envoy/proxy dep
    ],
    description_file = "tools/deb/description",
    homepage = "http://istio.io",
    maintainer = "The Istio Authors <istio-dev@googlegroups.com>",
    package = "istio",
    postinst = "tools/deb/postinst.sh",
    version = "0.2.1",
)
