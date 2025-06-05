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

"""Python toolchain module extensions for use with bzlmod.

::::{topic} Basic usage

The simplest way to configure the toolchain with `rules_python` is as follows.

```starlark
python = use_extension("@rules_python//python/extensions:python.bzl", "python")
python.toolchain(
    is_default = True,
    python_version = "3.11",
)
use_repo(python, "python_3_11")
```

:::{seealso}
For more in-depth documentation see the {obj}`python.toolchain`.
:::
::::

::::{topic} Overrides

Overrides can be done at 3 different levels:
* Overrides affecting all python toolchain versions on all platforms - {obj}`python.override`.
* Overrides affecting a single toolchain versions on all platforms - {obj}`python.single_version_override`.
* Overrides affecting a single toolchain versions on a single platforms - {obj}`python.single_version_platform_override`.

:::{seealso}
The main documentation page on registering [toolchains](/toolchains).
:::
::::
"""

load("//python/private:python.bzl", _python = "python")

python = _python
