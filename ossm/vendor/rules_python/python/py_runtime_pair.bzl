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

"""Public entry point for py_runtime_pair."""

load("@bazel_tools//tools/python:toolchain.bzl", _bazel_tools_impl = "py_runtime_pair")
load("//python/private:py_runtime_pair_macro.bzl", _starlark_impl = "py_runtime_pair")
load("//python/private:util.bzl", "IS_BAZEL_6_OR_HIGHER")

_py_runtime_pair = _starlark_impl if IS_BAZEL_6_OR_HIGHER else _bazel_tools_impl

# NOTE: This doc is copy/pasted from the builtin py_runtime_pair rule so our
# doc generator gives useful API docs.
def py_runtime_pair(name, py2_runtime = None, py3_runtime = None, **attrs):
    """A toolchain rule for Python.

    This is a macro around the underlying {rule}`py_runtime_pair` rule.

    This used to wrap up to two Python runtimes, one for Python 2 and one for Python 3.
    However, Python 2 is no longer supported, so it now only wraps a single Python 3
    runtime.

    Usually the wrapped runtimes are declared using the `py_runtime` rule, but any
    rule returning a `PyRuntimeInfo` provider may be used.

    This rule returns a `platform_common.ToolchainInfo` provider with the following
    schema:

    ```python
    platform_common.ToolchainInfo(
        py2_runtime = None,
        py3_runtime = <PyRuntimeInfo or None>,
    )
    ```

    Example usage:

    ```python
    # In your BUILD file...

    load("@rules_python//python:py_runtime.bzl", "py_runtime")
    load("@rules_python//python:py_runtime_pair.bzl", "py_runtime_pair")

    py_runtime(
        name = "my_py3_runtime",
        interpreter_path = "/system/python3",
        python_version = "PY3",
    )

    py_runtime_pair(
        name = "my_py_runtime_pair",
        py3_runtime = ":my_py3_runtime",
    )

    toolchain(
        name = "my_toolchain",
        target_compatible_with = <...>,
        toolchain = ":my_py_runtime_pair",
        toolchain_type = "@rules_python//python:toolchain_type",
    )
    ```

    ```python
    # In your WORKSPACE...

    register_toolchains("//my_pkg:my_toolchain")
    ```

    Args:
        name: str, the name of the target
        py2_runtime: optional Label; must be unset or None; an error is raised
            otherwise.
        py3_runtime: Label; a target with `PyRuntimeInfo` for Python 3.
        **attrs: Extra attrs passed onto the native rule
    """
    if attrs.get("py2_runtime"):
        fail("PYthon 2 is no longer supported: see https://github.com/bazel-contrib/rules_python/issues/886")
    _py_runtime_pair(
        name = name,
        py2_runtime = py2_runtime,
        py3_runtime = py3_runtime,
        **attrs
    )
