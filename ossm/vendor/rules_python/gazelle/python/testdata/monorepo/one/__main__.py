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

import os

import boto3
from bar import bar
from bar.baz import baz
from foo import foo

_ = boto3

if __name__ == "__main__":
    INIT_FILENAME = "__init__.py"
    dirname = os.path.dirname(os.path.abspath(__file__))
    assert bar() == os.path.join(dirname, "bar", INIT_FILENAME)
    assert baz() == os.path.join(dirname, "bar", "baz", INIT_FILENAME)
    assert foo() == os.path.join(dirname, "foo", INIT_FILENAME)
