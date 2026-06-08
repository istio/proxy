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

"""Implementation of current_py_cc_headers and current_py_cc_headers_abi3 rules.
"""

load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load("//python/private:toolchain_types.bzl", "PY_CC_TOOLCHAIN_TYPE")

def _current_py_cc_headers_impl(ctx):
    py_cc_toolchain = ctx.toolchains[PY_CC_TOOLCHAIN_TYPE].py_cc_toolchain
    return py_cc_toolchain.headers.providers_map.values()

current_py_cc_headers = rule(
    implementation = _current_py_cc_headers_impl,
    toolchains = [PY_CC_TOOLCHAIN_TYPE],
    provides = [CcInfo],
    doc = """
Provides the currently active Python toolchain's C headers.

This is a wrapper around the underlying `cc_library()` for the
C headers for the consuming target's currently active Python toolchain.

To use, simply depend on this target where you would have wanted the
toolchain's underlying `:python_headers` target:

```starlark
cc_library(
    name = "foo",
    deps = ["@rules_python//python/cc:current_py_cc_headers"]
)
```
""",
)

def _current_py_cc_headers_abi3_impl(ctx):
    py_cc_toolchain = ctx.toolchains[PY_CC_TOOLCHAIN_TYPE].py_cc_toolchain
    if not py_cc_toolchain.headers_abi3:
        fail((
            "The resolved {} toolchain does not provide abi3 headers. " +
            "Verify the toolchain sets `.headers_abi3`, or use the " +
            "`:current_py_cc_headers` target."
        ).format(
            PY_CC_TOOLCHAIN_TYPE,
        ))
    return py_cc_toolchain.headers_abi3.providers_map.values()

current_py_cc_headers_abi3 = rule(
    implementation = _current_py_cc_headers_abi3_impl,
    toolchains = [PY_CC_TOOLCHAIN_TYPE],
    provides = [CcInfo],
    doc = """
Provides the currently active Python toolchain's C ABI3 headers.

This is a wrapper around the underlying `cc_library()` for the
C ABI3 headers for the consuming target's currently active Python toolchain.

To use, simply depend on this target where you would have wanted the
toolchain's underlying `:python_headers_abi3` target:

```starlark
cc_library(
    name = "foo",
    deps = ["@rules_python//python/cc:current_py_cc_headers_abi3"]
)
```

:::{versionadded} 1.7.0
:::
""",
)
