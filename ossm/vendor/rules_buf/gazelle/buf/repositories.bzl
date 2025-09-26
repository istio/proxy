# Copyright 2021-2025 Buf Technologies, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Dependencies required to use buf gazelle extension."""

load("@bazel_gazelle//:deps.bzl", "go_repository")

def gazelle_buf_dependencies():
    """Utility method to load dependecies of the buf gazelle extension"""

    go_repository(
        name = "com_github_bazelbuild_bazel_gazelle",
        importpath = "github.com/bazelbuild/bazel-gazelle",
        sum = "h1:umlCZxzCEr8BHdesfY2sv5T8NTv9+JiPL8f/Vk83Aag=",
        version = "v0.25.0",
    )
    go_repository(
        name = "in_gopkg_yaml_v3",
        importpath = "gopkg.in/yaml.v3",
        sum = "h1:fxVm/GzAzEWqLHuvctI91KS9hhNmmWOoWu0XTYJS7CA=",
        version = "v3.0.1",
    )
    go_repository(
        name = "com_github_bazelbuild_buildtools",
        importpath = "github.com/bazelbuild/buildtools",
        sum = "h1:fmdo+fvvWlhldUcqkhAMpKndSxMN3vH5l7yow5cEaiQ=",
        version = "v0.0.0-20220531122519-a43aed7014c8",
    )
