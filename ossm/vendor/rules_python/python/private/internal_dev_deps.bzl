# Copyright 2024 The Bazel Authors. All rights reserved.
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
"""Module extension for internal dev_dependency=True setup."""

load("@bazel_ci_rules//:rbe_repo.bzl", "rbe_preconfig")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")

def _internal_dev_deps_impl(mctx):
    _ = mctx  # @unused

    # This wheel is purely here to validate the wheel extraction code. It's not
    # intended for anything else.
    http_file(
        name = "wheel_for_testing",
        downloaded_file_path = "numpy-1.25.2-cp311-cp311-manylinux_2_17_aarch64.manylinux2014_aarch64.whl",
        sha256 = "0d60fbae8e0019865fc4784745814cff1c421df5afee233db6d88ab4f14655a2",
        urls = [
            "https://files.pythonhosted.org/packages/50/67/3e966d99a07d60a21a21d7ec016e9e4c2642a86fea251ec68677daf71d4d/numpy-1.25.2-cp311-cp311-manylinux_2_17_aarch64.manylinux2014_aarch64.whl",
        ],
    )

    # Creates a default toolchain config for RBE.
    # Use this as is if you are using the rbe_ubuntu16_04 container,
    # otherwise refer to RBE docs.
    rbe_preconfig(
        name = "buildkite_config",
        toolchain = "ubuntu1804-bazel-java11",
    )

internal_dev_deps = module_extension(
    implementation = _internal_dev_deps_impl,
    doc = "This extension creates internal rules_python dev dependencies.",
)
