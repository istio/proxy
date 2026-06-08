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

"""Public entry point for py_binary."""

load("//python/private:py_binary_macro.bzl", _py_binary = "py_binary")

def py_binary(**attrs):
    """Creates an executable Python program.

    This is the public macro wrapping the underlying rule. Args are forwarded
    on as-is unless otherwise specified. See the underlying {rule}`py_binary`
    rule for detailed attribute documentation.

    This macro affects the following args:
    * `python_version`: cannot be `PY2`
    * `srcs_version`: cannot be `PY2` or `PY2ONLY`
    * `tags`: May have special marker values added, if not already present.

    :::{versionchanged} 1.9.0
    The `PYTHONBREAKPOINT` environment variable is inherited. Use in combination
    with {obj}`--debugger` to customize the debugger available and used.
    :::

    Args:
      **attrs: Rule attributes forwarded onto the underlying {rule}`py_binary`.
    """
    if attrs.get("python_version") == "PY2":
        fail("Python 2 is no longer supported: https://github.com/bazel-contrib/rules_python/issues/886")
    if attrs.get("srcs_version") in ("PY2", "PY2ONLY"):
        fail("Python 2 is no longer supported: https://github.com/bazel-contrib/rules_python/issues/886")

    _py_binary(**attrs)
