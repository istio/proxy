# Copyright 2024 The Bazel Authors. All rights reserved.
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

import os
import subprocess
import sys

print("outer sys.path:")
for i, x in enumerate(sys.path):
    print(i, x)
print()

outer_paths = set(sys.path)
output = subprocess.check_output(
    [
        sys.executable,
        "-c",
        "import sys; [print(v) for v in sys.path]",
    ],
    text=True,
)
inner_lines = [v for v in output.splitlines() if v.strip()]
print("inner sys.path:")
for i, v in enumerate(inner_lines):
    print(i, v)
print()

inner_paths = set(inner_lines)
common = sorted(outer_paths.intersection(inner_paths))
extra_outer = sorted(outer_paths - inner_paths)
extra_inner = sorted(inner_paths - outer_paths)

for v in common:
    print("common:", v)
print()
for v in extra_outer:
    print("extra_outer:", v)
print()
for v in extra_inner:
    print("extra_inner:", v)
