# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

""

load("//python/private/pypi:package_annotation.bzl", _package_annotation = "package_annotation")
load("//python/private/pypi:pip_repository.bzl", _pip_repository = "pip_repository")
load("//python/private/pypi:whl_config_repo.bzl", _whl_config_repo = "whl_config_repo")
load("//python/private/pypi:whl_library.bzl", _whl_library = "whl_library")

# Re-exports for backwards compatibility
group_library = _whl_config_repo
pip_repository = _pip_repository
whl_library = _whl_library
whl_config_repo = _whl_config_repo
package_annotation = _package_annotation
