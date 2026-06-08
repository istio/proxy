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

"""Implementation of current_node_cc_headers rule."""

def _current_node_cc_headers_impl(ctx):
    return ctx.toolchains["//nodejs:toolchain_type"].nodeinfo.headers.providers_map.values()

current_node_cc_headers = rule(
    implementation = _current_node_cc_headers_impl,
    toolchains = ["//nodejs:toolchain_type"],
    provides = [CcInfo],
    doc = """\
Provides the currently active Node toolchain's C++ headers.

This is a wrapper around the underlying `cc_library()` for the
C headers for the consuming target's currently active Node toolchain.

Note, "node.h" is only usable from C++, and you'll need to
ensure you are compiling with c++14 or later.

Also, on Windows, Node.js releases do not ship headers, so this rule is currently
not usable with the built-in toolchains. If you define your own toolchain on Windows,
you can include the headers and then this rule will work.

To use, simply depend on this target where you would have wanted the
toolchain's underlying `:headers` target:

```starlark
cc_library(
    name = "foo",
    srcs = ["foo.cc"],
    # If toolchain sets this already, you can omit.
    copts = ["-std=c++14"],
    deps = ["@rules_nodejs//nodejs/headers:current_node_cc_headers"]
)
```
""",
)
