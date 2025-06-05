# Copyright 2019 The Bazel Authors. All rights reserved.
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

"""Common build setting rules

These rules return a BuildSettingInfo with the value of the build setting.
For label-typed settings, use the native label_flag and label_setting rules.

More documentation on how to use build settings at
https://bazel.build/extending/config#user-defined-build-settings
"""

BuildSettingInfo = provider(
    doc = "A singleton provider that contains the raw value of a build setting",
    fields = {
        "value": "The value of the build setting in the current configuration. " +
                 "This value may come from the command line or an upstream transition, " +
                 "or else it will be the build setting's default.",
    },
)

_MAKE_VARIABLE_ATTR = attr.string(
    doc = "If set, the build setting's value will be available as a Make variable with this " +
          "name in the attributes of rules that list this build setting in their 'toolchains' " +
          "attribute.",
)

def _is_valid_make_variable_char(c):
    # Restrict make variable names for consistency with predefined ones. There are no enforced
    # restrictions on make variable names, but when they contain e.g. spaces or braces, they
    # aren't expanded by e.g. cc_binary.
    return c == "_" or c.isdigit() or (c.isalpha() and c.isupper())

def _get_template_variable_info(ctx):
    make_variable = getattr(ctx.attr, "make_variable", None)
    if not make_variable:
        return []

    if not all([_is_valid_make_variable_char(c) for c in make_variable.elems()]):
        fail("Error setting " + _no_at_str(ctx.label) + ": invalid make variable name '" + make_variable + "'. Make variable names may only contain uppercase letters, digits, and underscores.")

    return [
        platform_common.TemplateVariableInfo({
            make_variable: str(ctx.build_setting_value),
        }),
    ]

def _impl(ctx):
    return [
        BuildSettingInfo(value = ctx.build_setting_value),
    ] + _get_template_variable_info(ctx)

int_flag = rule(
    implementation = _impl,
    build_setting = config.int(flag = True),
    attrs = {
        "make_variable": _MAKE_VARIABLE_ATTR,
    },
    doc = "An int-typed build setting that can be set on the command line",
)

int_setting = rule(
    implementation = _impl,
    build_setting = config.int(),
    attrs = {
        "make_variable": _MAKE_VARIABLE_ATTR,
    },
    doc = "An int-typed build setting that cannot be set on the command line",
)

bool_flag = rule(
    implementation = _impl,
    build_setting = config.bool(flag = True),
    doc = "A bool-typed build setting that can be set on the command line",
)

bool_setting = rule(
    implementation = _impl,
    build_setting = config.bool(),
    doc = "A bool-typed build setting that cannot be set on the command line",
)

string_list_flag = rule(
    implementation = _impl,
    build_setting = config.string_list(flag = True),
    doc = "A string list-typed build setting that can be set on the command line",
)

string_list_setting = rule(
    implementation = _impl,
    build_setting = config.string_list(),
    doc = "A string list-typed build setting that cannot be set on the command line",
)

def _no_at_str(label):
    """Strips any leading '@'s for labels in the main repo, so that the error string is more friendly."""
    s = str(label)
    if s.startswith("@@//"):
        return s[2:]
    if s.startswith("@//"):
        return s[1:]
    return s

def _string_impl(ctx):
    allowed_values = ctx.attr.values
    value = ctx.build_setting_value
    if len(allowed_values) == 0 or value in ctx.attr.values:
        return [BuildSettingInfo(value = value)] + _get_template_variable_info(ctx)
    else:
        fail("Error setting " + _no_at_str(ctx.label) + ": invalid value '" + value + "'. Allowed values are " + str(allowed_values))

string_flag = rule(
    implementation = _string_impl,
    build_setting = config.string(flag = True),
    attrs = {
        "values": attr.string_list(
            doc = "The list of allowed values for this setting. An error is raised if any other value is given.",
        ),
        "make_variable": _MAKE_VARIABLE_ATTR,
    },
    doc = "A string-typed build setting that can be set on the command line",
)

string_setting = rule(
    implementation = _string_impl,
    build_setting = config.string(),
    attrs = {
        "values": attr.string_list(
            doc = "The list of allowed values for this setting. An error is raised if any other value is given.",
        ),
        "make_variable": _MAKE_VARIABLE_ATTR,
    },
    doc = "A string-typed build setting that cannot be set on the command line",
)
