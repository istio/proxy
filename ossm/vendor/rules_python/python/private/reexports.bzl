# Copyright 2019 The Bazel Authors. All rights reserved.
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

"""Internal re-exports of builtin symbols.

We want to use both the PyInfo defined by builtins and the one defined by
rules_python. Because the builtin symbol is going away, the rules_python
PyInfo symbol is given preference. Unfortunately, that masks the builtin,
so we have to rebind it to another name and load it to make it available again.

Unfortunately, we can't just write:

```
PyInfo = PyInfo
```

because the declaration of module-level symbol `PyInfo` makes the builtin
inaccessible. So instead we access the builtin here and export it under a
different name. Then we can load it from elsewhere.
"""

load("@rules_python_internal//:rules_python_config.bzl", "config")

# NOTE: May be None (Bazel 8 autoloading rules_python)
# buildifier: disable=name-conventions
BuiltinPyInfo = config.BuiltinPyInfo

# NOTE: May be None (Bazel 8 autoloading rules_python)
# buildifier: disable=name-conventions
BuiltinPyRuntimeInfo = config.BuiltinPyRuntimeInfo
