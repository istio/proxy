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

import sys

import bar
import baz
import foo

# Ensure that even though @gazelle_python_test//other_pip_dep provides "third_party",
# we can still override "third_party.foo.bar"
import third_party.foo.bar

from third_party import baz

import third_party

_ = sys
_ = bar
_ = baz
_ = foo
