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
Creates an executable (a non-test binary) for console_script entry points.

```{include} /_includes/py_console_script_binary.md
```
"""

load("//python/private:py_console_script_binary.bzl", _py_console_script_binary = "py_console_script_binary")

py_console_script_binary = _py_console_script_binary
