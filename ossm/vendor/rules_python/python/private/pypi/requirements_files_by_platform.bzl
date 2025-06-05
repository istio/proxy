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

"""Get the requirement files by platform."""

load(":whl_target_platforms.bzl", "whl_target_platforms")

# TODO @aignas 2024-05-13: consider using the same platform tags as are used in
# the //python:versions.bzl
DEFAULT_PLATFORMS = [
    "linux_aarch64",
    "linux_arm",
    "linux_ppc",
    "linux_s390x",
    "linux_x86_64",
    "osx_aarch64",
    "osx_x86_64",
    "windows_x86_64",
]

def _default_platforms(*, filter):
    if not filter:
        fail("Must specific a filter string, got: {}".format(filter))

    if filter.startswith("cp3"):
        # TODO @aignas 2024-05-23: properly handle python versions in the filter.
        # For now we are just dropping it to ensure that we don't fail.
        _, _, filter = filter.partition("_")

    sanitized = filter.replace("*", "").replace("_", "")
    if sanitized and not sanitized.isalnum():
        fail("The platform filter can only contain '*', '_' and alphanumerics")

    if "*" in filter:
        prefix = filter.rstrip("*")
        if "*" in prefix:
            fail("The filter can only contain '*' at the end of it")

        if not prefix:
            return DEFAULT_PLATFORMS

        return [p for p in DEFAULT_PLATFORMS if p.startswith(prefix)]
    else:
        return [p for p in DEFAULT_PLATFORMS if filter in p]

def _platforms_from_args(extra_pip_args):
    platform_values = []

    if not extra_pip_args:
        return platform_values

    for arg in extra_pip_args:
        if platform_values and platform_values[-1] == "":
            platform_values[-1] = arg
            continue

        if arg == "--platform":
            platform_values.append("")
            continue

        if not arg.startswith("--platform"):
            continue

        _, _, plat = arg.partition("=")
        if not plat:
            _, _, plat = arg.partition(" ")
        if plat:
            platform_values.append(plat)
        else:
            platform_values.append("")

    if not platform_values:
        return []

    platforms = {
        p.target_platform: None
        for arg in platform_values
        for p in whl_target_platforms(arg)
    }
    return list(platforms.keys())

def _platform(platform_string, python_version = None):
    if not python_version or platform_string.startswith("cp3"):
        return platform_string

    _, _, tail = python_version.partition(".")
    minor, _, _ = tail.partition(".")

    return "cp3{}_{}".format(minor, platform_string)

def requirements_files_by_platform(
        *,
        requirements_by_platform = {},
        requirements_osx = None,
        requirements_linux = None,
        requirements_lock = None,
        requirements_windows = None,
        extra_pip_args = None,
        python_version = None,
        logger = None,
        fail_fn = fail):
    """Resolve the requirement files by target platform.

    Args:
        requirements_by_platform (label_keyed_string_dict): a way to have
            different package versions (or different packages) for different
            os, arch combinations.
        requirements_osx (label): The requirements file for the osx OS.
        requirements_linux (label): The requirements file for the linux OS.
        requirements_lock (label): The requirements file for all OSes, or used as a fallback.
        requirements_windows (label): The requirements file for windows OS.
        extra_pip_args (string list): Extra pip arguments to perform extra validations and to
            be joined with args fined in files.
        python_version: str or None. This is needed when the get_index_urls is
            specified. It should be of the form "3.x.x",
        logger: repo_utils.logger or None, a simple struct to log diagnostic messages.
        fail_fn (Callable[[str], None]): A failure function used in testing failure cases.

    Returns:
        A dict with keys as the labels to the files and values as lists of
        platforms that the files support.
    """
    if not (
        requirements_lock or
        requirements_linux or
        requirements_osx or
        requirements_windows or
        requirements_by_platform
    ):
        fail_fn(
            "A 'requirements_lock' attribute must be specified, a platform-specific lockfiles " +
            "via 'requirements_by_platform' or an os-specific lockfiles must be specified " +
            "via 'requirements_*' attributes",
        )
        return None

    platforms = _platforms_from_args(extra_pip_args)
    if logger:
        logger.debug(lambda: "Platforms from pip args: {}".format(platforms))

    if platforms:
        lock_files = [
            f
            for f in [
                requirements_lock,
                requirements_linux,
                requirements_osx,
                requirements_windows,
            ] + list(requirements_by_platform.keys())
            if f
        ]

        if len(lock_files) > 1:
            # If the --platform argument is used, check that we are using
            # a single `requirements_lock` file instead of the OS specific ones as that is
            # the only correct way to use the API.
            fail_fn("only a single 'requirements_lock' file can be used when using '--platform' pip argument, consider specifying it via 'requirements_lock' attribute")
            return None

        files_by_platform = [
            (lock_files[0], platforms),
        ]
        if logger:
            logger.debug(lambda: "Files by platform with the platform set in the args: {}".format(files_by_platform))
    else:
        files_by_platform = {
            file: [
                platform
                for filter_or_platform in specifier.split(",")
                for platform in (_default_platforms(filter = filter_or_platform) if filter_or_platform.endswith("*") else [filter_or_platform])
            ]
            for file, specifier in requirements_by_platform.items()
        }.items()

        if logger:
            logger.debug(lambda: "Files by platform with the platform set in the attrs: {}".format(files_by_platform))

        for f in [
            # If the users need a greater span of the platforms, they should consider
            # using the 'requirements_by_platform' attribute.
            (requirements_linux, _default_platforms(filter = "linux_*")),
            (requirements_osx, _default_platforms(filter = "osx_*")),
            (requirements_windows, _default_platforms(filter = "windows_*")),
            (requirements_lock, None),
        ]:
            if f[0]:
                if logger:
                    logger.debug(lambda: "Adding an extra item to files_by_platform: {}".format(f))
                files_by_platform.append(f)

    configured_platforms = {}
    requirements = {}
    for file, plats in files_by_platform:
        if plats:
            plats = [_platform(p, python_version) for p in plats]
            for p in plats:
                if p in configured_platforms:
                    fail_fn(
                        "Expected the platform '{}' to be map only to a single requirements file, but got multiple: '{}', '{}'".format(
                            p,
                            configured_platforms[p],
                            file,
                        ),
                    )
                    return None

                configured_platforms[p] = file
        else:
            default_platforms = [_platform(p, python_version) for p in DEFAULT_PLATFORMS]
            plats = [
                p
                for p in default_platforms
                if p not in configured_platforms
            ]
            if logger:
                logger.debug(lambda: "File {} will be used for the remaining platforms {} that are not in configured_platforms: {}".format(
                    file,
                    plats,
                    default_platforms,
                ))
            for p in plats:
                configured_platforms[p] = file

        if logger:
            logger.debug(lambda: "Configured platforms for file {} are {}".format(file, plats))

        for p in plats:
            if p in requirements:
                # This should never happen because in the code above we should
                # have unambiguous selection of the requirements files.
                fail_fn("Attempting to override a requirements file '{}' with '{}' for platform '{}'".format(
                    requirements[p],
                    file,
                    p,
                ))
                return None
            requirements[p] = file

    # Now return a dict that is similar to requirements_by_platform - where we
    # have labels/files as keys in the dict to minimize the number of times we
    # may parse the same file.

    ret = {}
    for plat, file in requirements.items():
        ret.setdefault(file, []).append(plat)

    return ret
