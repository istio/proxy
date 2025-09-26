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

"""Implementation of py_runtime_pair macro portion."""

load(":py_runtime_pair_rule.bzl", _py_runtime_pair = "py_runtime_pair")

# A fronting macro is used because macros have user-observable behavior;
# using one from the onset avoids introducing those changes in the future.
def py_runtime_pair(**kwargs):
    """Creates a py_runtime_pair target.

    Args:
        **kwargs: Keyword args to pass onto underlying rule.
    """
    _py_runtime_pair(**kwargs)
