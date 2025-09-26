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

"""
The {obj}`config_settings` macro is used to create the config setting targets
that can be used in the {obj}`pkg_aliases` macro for selecting the compatible
repositories.

The config settings are of the form `:is_cp3<minor_version><suffix>`. Suffix is a 
normalized user provided value for the platform name. The goal of this macro is to
ensure that we can incorporate the user provided `config_setting` targets to create
our composite config_setting targets.
"""

load("@bazel_skylib//lib:selects.bzl", "selects")

def config_settings(
        *,
        python_versions = [],
        name = None,
        platform_config_settings = {},
        **kwargs):
    """Generate all of the pip config settings.

    Args:
        name (str): Currently unused.
        python_versions (list[str]): The list of python versions to configure
            config settings for.
        platform_config_settings: {type}`dict[str, list[str]]` the constraint
            values to use instead of the default ones. Key are platform names
            (a human-friendly platform string). Values are lists of
            `constraint_value` label strings.
        **kwargs: Other args passed to the underlying implementations, such as
            {obj}`native`.
    """
    target_platforms = {
        "": [],
    } | platform_config_settings

    for platform_name, config_settings in target_platforms.items():
        suffix = "_{}".format(platform_name) if platform_name else ""

        # We parse the target settings and if there is a "platforms//os" or
        # "platforms//cpu" value in here, we also add it into the constraint_values
        #
        # this is to ensure that we can still pass all of the unit tests for config
        # setting specialization.
        #
        # TODO @aignas 2025-07-23: is this the right way? Maybe we should drop these
        # and remove the tests?
        constraint_values = []
        for setting in config_settings:
            setting_label = Label(setting)
            if setting_label.repo_name == "platforms" and setting_label.package in ["os", "cpu"]:
                constraint_values.append(setting)

        for python_version in python_versions:
            cpv = "cp" + python_version.replace(".", "")
            prefix = "is_{}".format(cpv)

            _dist_config_setting(
                name = prefix + suffix,
                flag_values = {
                    Label("//python/config_settings:python_version_major_minor"): python_version,
                },
                config_settings = config_settings,
                constraint_values = constraint_values,
                **kwargs
            )

def _dist_config_setting(*, name, selects = selects, native = native, config_settings = None, **kwargs):
    """A macro to create a target for matching Python binary and source distributions.

    Args:
        name: The name of the public target.
        config_settings: {type}`list[str | Label]` the list of target settings that must
            be matched before we try to evaluate the config_setting that we may create in
            this function.
        selects (struct): The struct containing config_setting_group function
            to use for creating config setting groups. Can be overridden for unit tests
            reasons.
        native (struct): The struct containing alias and config_setting rules
            to use for creating the objects. Can be overridden for unit tests
            reasons.
        **kwargs: The kwargs passed to the config_setting rule. Visibility of
            the main alias target is also taken from the kwargs.
    """

    # first define the config setting that has all of the constraint values
    _name = "_" + name
    native.config_setting(
        name = _name,
        **kwargs
    )
    selects.config_setting_group(
        name = name,
        match_all = config_settings + [_name],
        visibility = kwargs.get("visibility"),
    )
