# Copyright 2021 The Bazel Authors. All rights reserved.
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

"""Custom build settings rules for Swift rules."""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")

def _repeatable_string_flag_impl(ctx):
    return BuildSettingInfo(value = ctx.build_setting_value)

repeatable_string_flag = rule(
    build_setting = config.string_list(
        flag = True,
        repeatable = True,
    ),
    doc = """\
A string-valued flag that can occur on the command line multiple times, used for
flags like `copt`. This allows flags to be stacked in `--config`s (rather than
overwriting previous occurrences) and also makes no assumption about the values
of the flags (comma-splitting does not occur).
""",
    implementation = _repeatable_string_flag_impl,
)

PerModuleSwiftCoptSettingInfo = provider(
    doc = "A provider for the parsed per-swiftmodule swift copts.",
    fields = {
        "value": "A map of target labels to lists of additional Swift copts to apply to" +
                 "the target.",
    },
)

def additional_per_module_swiftcopts(label, provider):
    """Returns additional swiftcopts to apply to a target named `label`.

    Args:
      label: The label of the target.
      provider: The `PerModuleSwiftCoptsSettingInfo` provider from the calling
          context.
    Returns:
        A list of additional swiftcopts to use when builing the target.
    """
    per_module_copts = provider.value
    if per_module_copts:
        target_label = str(label)
        return per_module_copts.get(target_label, [])
    return []

def _per_module_swiftcopt_flag_impl(ctx):
    # Each item in this list should of the form
    # "<target label>=<comma separated copts>".
    module_and_copts_list = ctx.build_setting_value
    value = dict()
    for item in module_and_copts_list:
        if not item:
            continue
        contents = item.split("=", 1)
        if len(contents) != 2:
            fail("""\
--per_module_swiftcopt must be written as a target label, an equal sign \
("="), and a comma-delimited list of compiler flags. For example, \
"//package:target=-copt,-copt,...".""")

        # Canonicalize the label using the `Label` constructor.
        label = str(Label(contents[0]))
        raw_copts = contents[1]

        # TODO(b/186875113): This breaks if any of the copts actually use
        # commas. Switch to a more selective approach to splitting,
        # respecting an escape sequence for commas inside of copts.
        copts = raw_copts.split(",")
        if len(copts) > 0:
            existing_copts = value.get(label)
            if existing_copts:
                existing_copts.extend(copts)
            else:
                value[label] = copts
    return PerModuleSwiftCoptSettingInfo(value = value)

per_module_swiftcopt_flag = rule(
    build_setting = config.string(
        flag = True,
        allow_multiple = True,
    ),
    # TODO(b/186869451): Support adding swiftcopts by module name in addition
    # to the target label.
    doc = """\
A string list build setting that can be set on the command line. Each item in
the list is expected to be of the form: <//target-package:name>=<copts> where
copts is a colon separated list of Swift copts.
""",
    implementation = _per_module_swiftcopt_flag_impl,
)
