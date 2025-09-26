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

"""Public entry point for py_runtime."""

load("//python/private:py_runtime_macro.bzl", _starlark_py_runtime = "py_runtime")
load("//python/private:util.bzl", "IS_BAZEL_6_OR_HIGHER", "add_migration_tag")

# buildifier: disable=native-python
_py_runtime_impl = _starlark_py_runtime if IS_BAZEL_6_OR_HIGHER else native.py_runtime

def py_runtime(**attrs):
    """Creates an executable Python program.

    This is the public macro wrapping the underlying rule. Args are forwarded
    on as-is unless otherwise specified. See
    {rule}`py_runtime`
    for detailed attribute documentation.

    This macro affects the following args:
    * `python_version`: cannot be `PY2`
    * `srcs_version`: cannot be `PY2` or `PY2ONLY`
    * `tags`: May have special marker values added, if not already present.

    Args:
      **attrs: Rule attributes forwarded onto {rule}`py_runtime`.
    """
    if attrs.get("python_version") == "PY2":
        fail("Python 2 is no longer supported: see https://github.com/bazel-contrib/rules_python/issues/886")

    _py_runtime_impl(**add_migration_tag(attrs))
