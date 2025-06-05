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
"""Providers for transitively gathering all license and package_info targets.

Warning: This is private to the aspect that walks the tree. The API is subject
to change at any release.
"""

LicensedTargetInfo = provider(
    doc = """Lists the licenses directly used by a single target.""",
    fields = {
        "target_under_license": "Label: The target label",
        "licenses": "list(label of a license rule)",
    },
)

def licenses_info():
    return provider(
        doc = """The transitive set of licenses used by a target.""",
        fields = {
            "target_under_license": "Label: The top level target label.",
            "deps": "depset(LicensedTargetInfo): The transitive list of dependencies that have licenses.",
            "licenses": "depset(LicenseInfo)",
            "traces": "list(string) - diagnostic for tracing a dependency relationship to a target.",
        },
    )

# This provider is used by the aspect that is used by manifest() rules.
TransitiveLicensesInfo = licenses_info()

TransitiveMetadataInfo = provider(
    doc = """The transitive set of licenses used by a target.""",
    fields = {
        "top_level_target": "Label: The top level target label we are examining.",
        "other_metadata": "depset(ExperimentalMetatdataInfo)",
        "licenses": "depset(LicenseInfo)",
        "package_info": "depset(PackageInfo)",

        "target_under_license": "Label: A target which will be associated with some licenses.",
        "deps": "depset(LicensedTargetInfo): The transitive list of dependencies that have licenses.",
        "traces": "list(string) - diagnostic for tracing a dependency relationship to a target.",
    },
)
