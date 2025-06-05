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

"""Common attributes between bzlmod pip.parse and workspace pip_parse.

A common attributes shared between bzlmod and workspace implementations
stored in a separate file to avoid unnecessary refetching of the
repositories."""

load(":attrs.bzl", COMMON_ATTRS = "ATTRS")

ATTRS = {
    "requirements_by_platform": attr.label_keyed_string_dict(
        doc = """\
The requirements files and the comma delimited list of target platforms as values.

The keys are the requirement files and the values are comma-separated platform
identifiers. For now we only support `<os>_<cpu>` values that are present in
`@platforms//os` and `@platforms//cpu` packages respectively.
""",
    ),
    "requirements_darwin": attr.label(
        allow_single_file = True,
        doc = "Override the requirements_lock attribute when the host platform is Mac OS",
    ),
    "requirements_linux": attr.label(
        allow_single_file = True,
        doc = "Override the requirements_lock attribute when the host platform is Linux",
    ),
    "requirements_lock": attr.label(
        allow_single_file = True,
        doc = """\
A fully resolved 'requirements.txt' pip requirement file containing the
transitive set of your dependencies. If this file is passed instead of
'requirements' no resolve will take place and pip_repository will create
individual repositories for each of your dependencies so that wheels are
fetched/built only for the targets specified by 'build/run/test'. Note that if
your lockfile is platform-dependent, you can use the `requirements_[platform]`
attributes.

Note, that in general requirements files are compiled for a specific platform,
but sometimes they can work for multiple platforms. `rules_python` right now
supports requirements files that are created for a particular platform without
platform markers.
""",
    ),
    "requirements_windows": attr.label(
        allow_single_file = True,
        doc = "Override the requirements_lock attribute when the host platform is Windows",
    ),
    "use_hub_alias_dependencies": attr.bool(
        default = False,
        doc = """\
Controls if the hub alias dependencies are used. If set to true, then the
group_library will be included in the hub repo.

True will become default in a subsequent release.
""",
    ),
}

ATTRS.update(**COMMON_ATTRS)
