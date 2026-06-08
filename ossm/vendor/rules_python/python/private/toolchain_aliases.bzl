# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""Create toolchain alias targets."""

load("@bazel_skylib//lib:selects.bzl", "selects")
load("//python:versions.bzl", "PLATFORMS")

def toolchain_aliases(*, name, platforms, visibility = None, native = native):
    """Create toolchain aliases for the python toolchains.

    Args:
        name: {type}`str` The name of the current repository.
        platforms: {type}`platforms` The list of platforms that are supported
            for the current toolchain repository.
        visibility: {type}`list[Target] | None` The visibility of the aliases.
        native: The native struct used in the macro, useful for testing.
    """
    for platform in PLATFORMS.keys():
        if platform not in platforms:
            continue

        _platform = "_" + platform
        native.config_setting(
            name = _platform,
            constraint_values = PLATFORMS[platform].compatible_with,
            visibility = ["//visibility:private"],
        )
        selects.config_setting_group(
            name = platform,
            match_all = PLATFORMS[platform].target_settings + [_platform],
            visibility = ["//visibility:private"],
        )

    prefix = name
    for name in [
        "files",
        "includes",
        "libpython",
        "py3_runtime",
        "python_headers",
        "python_runtimes",
    ]:
        native.alias(
            name = name,
            actual = select({
                ":" + platform: "@{}_{}//:{}".format(prefix, platform, name)
                for platform in platforms
            }),
            visibility = visibility,
        )

    native.alias(
        name = "python3",
        actual = select({
            ":" + platform: "@{}_{}//:{}".format(prefix, platform, "python.exe" if "windows" in platform else "bin/python3")
            for platform in platforms
        }),
        visibility = visibility,
    )
    native.alias(
        name = "pip",
        actual = select({
            ":" + platform: "@{}_{}//:python_runtimes".format(prefix, platform)
            for platform in platforms
            if "windows" not in platform
        }),
        visibility = visibility,
    )
