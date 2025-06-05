# Copyright 2022 The Bazel Authors. All rights reserved.
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

"""license_kind()s for generic license identifiers."""

# This is a set of license_kind declarations based on the legacy Bazel license
# identifiers.
#
# Projects using one of these licenses may reference the license_kind targets
# here.  For example, their BUILD file might contain:
#
#   package(default_applicable_licenses = [":license"])
#
#   license(
#       name = "license",
#       license_kinds = ["@rules_license//licenses/generic:notice"],
#       license_text = "LICENSE",
#   )
#

# These licenses represent the LicenseType enum originally baked into Bazel. See
# https://github.com/bazelbuild/bazel/blob/fcfca6157b6de903ab942cef2fc460bf8c8da6ed/src/main/java/com/google/devtools/build/lib/packages/License.java#L59
#
# Packages may create license rules that use these license_kinds when the
# package has provided a LICENSE file, but the text in it  does not match any
# of the well known licenses in @rules_license//licenses/spdx:*
load("@rules_license//rules:license_kind.bzl", "license_kind")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

filegroup(
    name = "standard_package",
    srcs = ["BUILD"],
)

# "none" should be used for packages which are distributed with no license of
# any kind. You can use this no-op license as a positive indication that the
# code's license terms were reviewed, so that linters will not flag it later as
# unreviewed.
license_kind(
    name = "none",
)

# See: https://opensource.google/docs/thirdparty/licenses/#unencumbered
license_kind(
    name = "unencumbered",
)

# See: https://opensource.google/docs/thirdparty/licenses/#notice
license_kind(
    name = "notice",
    conditions = [
        "notice",
    ],
)

# See: https://opensource.google/docs/thirdparty/licenses/#permissive
license_kind(
    name = "permissive",
    conditions = [
        "permissive",
    ],
)

# See: https://opensource.google/docs/thirdparty/licenses/#reciprocal
license_kind(
    name = "reciprocal",
    conditions = [
        "reciprocal",
    ],
)

# See: https://opensource.google/docs/thirdparty/licenses/#restricted
license_kind(
    name = "restricted",
    conditions = [
        "restricted",
    ],
)

# See: https://opensource.google/docs/thirdparty/licenses/#ByExceptionOnly
license_kind(
    name = "by_exception_only",
    conditions = [
        "by_exception_only",
    ],
)
