# Copyright 2023 The Bazel Authors. All rights reserved.
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
"""PYTHON RULE IMPLEMENTATION ONLY: Do not use outside of the rule implementations and their tests.

NOTE: This file is only loaded by @rules_python_internal//:py_internal.bzl. This
is because the `py_internal` global symbol is only present in Bazel 7+, so
a repo rule has to conditionally load this depending on the Bazel version.

Re-exports the restricted-use py_internal helper under another name. This is
necessary because `py_internal = py_internal` results in an error (trying
to bind a local symbol to itself before its defined).

This is to allow the rule implementation in the //python directory to access
the internal helpers only rules_python is allowed to use.

These may change at any time and are closely coupled to the rule implementation.
"""

py_internal_renamed = py_internal
