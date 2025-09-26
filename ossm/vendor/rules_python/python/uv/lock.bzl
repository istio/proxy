# Copyright 2025 The Bazel Authors. All rights reserved.
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

"""The `uv` locking rule.

Differences with the legacy {obj}`compile_pip_requirements` rule:
- This is implemented as a rule that performs locking in a build action.
- Additionally one can use the runnable target.
- Uses `uv`.
- This does not error out if the output file does not exist yet.
- Supports transitions out of the box.

Note, this does not provide a `test` target, if you would like to add a test
target that always does the locking automatically to ensure that the
`requirements.txt` file is up-to-date, add something similar to:

```starlark
load("@bazel_skylib//rules:native_binary.bzl", "native_test")
load("@rules_python//python/uv:lock.bzl", "lock")

lock(
    name = "requirements",
    srcs = ["pyproject.toml"],
)

native_test(
    name = "requirements_test",
    src = "requirements.update",
)
```

EXPERIMENTAL: This is experimental and may be changed without notice.
"""

load("//python/uv/private:lock.bzl", _lock = "lock")

lock = _lock
