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
"""Basic providers for license rules.

This file should only contain the basic providers needed to create
license and package_info declarations. Providers needed to gather
them are declared in other places.
"""

load(
    "@rules_license//rules_gathering:gathering_providers.bzl",
    _private_TransitiveLicensesInfo = "TransitiveLicensesInfo",
)

LicenseKindInfo = provider(
    doc = """Provides information about a license_kind instance.""",
    fields = {
        "conditions": "list(string): List of conditions to be met when using this packages under this license.",
        "label": "Label: The full path to the license kind definition.",
        "long_name": "string: Human readable license name",
        "name": "string: Canonical license name",
    },
)

LicenseInfo = provider(
    doc = """Provides information about a license instance.""",
    fields = {
        "copyright_notice": "string: Human readable short copyright notice",
        "label": "Label: label of the license rule",
        "license_kinds": "list(LicenseKindInfo): License kinds ",
        "license_text": "string: The license file path",
        # TODO(aiuto): move to PackageInfo
        "package_name": "string: Human readable package name",
        "package_url": "URL from which this package was downloaded.",
        "package_version": "Human readable version string",
    },
)

PackageInfo = provider(
    doc = """Provides information about a package.""",
    fields = {
        "type": "string: How to interpret data",
        "label": "Label: label of the package_info rule",
        "package_name": "string: Human readable package name",
        "package_url": "string: URL from which this package was downloaded.",
        "package_version": "string: Human readable version string",
        "purl": "string: package url matching the purl spec (https://github.com/package-url/purl-spec)",
    },
)

# This is more extensible. Because of the provider implementation, having a big
# dict of values rather than named fields is not much more costly.
# Design choice.  Replace data with actual providers, such as PackageInfo
ExperimentalMetadataInfo = provider(
    doc = """Generic bag of metadata.""",
    fields = {
        "type": "string: How to interpret data",
        "label": "Label: label of the metadata rule",
        "data": "String->any: Map of names to values",
    },
)

# Deprecated: Use write_licenses_info instead.
TransitiveLicensesInfo = _private_TransitiveLicensesInfo
