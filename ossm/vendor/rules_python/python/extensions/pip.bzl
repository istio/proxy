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

"""
This is the successor to {bzl:obj}`pip_parse` for including third party PyPI dependencies into your bazel module using `bzlmod`.

:::{seealso}
For user documentation see the [PyPI dependencies section](pypi-dependencies).
:::
"""

load("//python/private/pypi:pip.bzl", _pip = "pip")

pip = _pip
