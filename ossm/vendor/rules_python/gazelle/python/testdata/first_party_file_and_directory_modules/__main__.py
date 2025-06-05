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

import foo
from baz import baz as another_baz
from foo.bar import baz
from one.two import two
from package1.subpackage1.module1 import find_me

assert not hasattr(foo, "foo")
assert baz() == "baz from foo/bar.py"
assert another_baz() == "baz from baz.py"
assert two() == "two"
assert find_me() == "found"
