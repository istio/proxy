# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""A rule to define a target to act as a singleton for label attributes.

Label attributes with defaults cannot accept None, otherwise they fall
back to using the default. A sentinel allows detecting an intended None value.
"""

SentinelInfo = provider(
    doc = "Indicates this was the sentinel target.",
    fields = [],
)

def _sentinel_impl(ctx):
    _ = ctx  # @unused
    return [SentinelInfo()]

sentinel = rule(implementation = _sentinel_impl)
