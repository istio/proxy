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
import sys

print("Hello")
print(
    "RULES_PYTHON_ZIP_DIR:{}".format(sys._xoptions.get("RULES_PYTHON_ZIP_DIR", "UNSET"))
)
print("PYTHONSAFEPATH:", os.environ.get("PYTHONSAFEPATH", "UNSET") or "EMPTY")
print("sys.flags.safe_path:", sys.flags.safe_path)
print("file:", __file__)
print("sys.executable:", sys.executable)
