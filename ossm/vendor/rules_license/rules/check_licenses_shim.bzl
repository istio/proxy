# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""This module provides a custom Starlark rule used to create wrappers for targets that
can have blaze build --check_licenses executed against them."""

def _shim_rule_impl(ctx):
    # This rule doesn't need to return anything. It only exists to propagate the dependency supplied
    # by the label_flag
    return []

shim_rule = rule(
    doc = """This rule exists to configure a dependent target via label. An instantiation of this
    rule is then used as a dependency for the legacy_check_target rule, which can be built with --check_licenses
    to get the effect of running --check_licenses on an arbitrary target which may or may not have a distribs
    attribute""",
    implementation = _shim_rule_impl,
    # The definition of this attribute creates a dependency relationship on the manually provided label.
    attrs = {"target": attr.label(default = ":check_licenses_target")},
)
