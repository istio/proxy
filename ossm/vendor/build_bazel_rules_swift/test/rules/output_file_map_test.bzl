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

"""Rules for testing the output file maps."""

load("@bazel_skylib//lib:collections.bzl", "collections")
load("@bazel_skylib//lib:unittest.bzl", "analysistest", "unittest")

def _output_file_map_test_impl(ctx):
    env = analysistest.begin(ctx)
    target_under_test = analysistest.target_under_test(env)

    # Find the WriteFile action that outputs the file map.
    actions = analysistest.target_actions(env)
    output_file_map = ctx.attr.output_file_map
    action_outputs = [
        (action, [file.short_path for file in action.outputs.to_list()])
        for action in actions
    ]
    matching_actions = [
        action
        for action, outputs in action_outputs
        if output_file_map in outputs
    ]
    if not matching_actions:
        actual_outputs = collections.uniq([
            output.short_path
            for action in actions
            for output in action.outputs.to_list()
            if output.path.endswith(".output_file_map.json")
        ])
        unittest.fail(
            env,
            ("Target '{}' registered no actions that outputs '{}' " +
             "(it had {}).").format(
                str(target_under_test.label),
                output_file_map,
                actual_outputs,
            ),
        )
        return analysistest.end(env)
    if len(matching_actions) != 1:
        unittest.fail(
            env,
            ("Expected exactly one action that outputs '{}', " +
             "but found {}.").format(
                output_file_map,
                len(matching_actions),
            ),
        )
        return analysistest.end(env)

    action = matching_actions[0]
    message_prefix = "In {} action for output '{}' for target '{}', ".format(
        action.mnemonic,
        output_file_map,
        str(target_under_test.label),
    )

    content = json.decode(action.content)
    file_entry = ctx.attr.file_entry
    if file_entry not in content:
        unittest.fail(
            env,
            ("Output file map '{}' doesn't contain file entry '{}' " +
             "(it had {}).").format(
                output_file_map,
                file_entry,
                content.keys(),
            ),
        )
        return analysistest.end(env)

    file_map = content[file_entry]
    for expected_key, expected_value in ctx.attr.expected_mapping.items():
        if expected_key not in file_map:
            unittest.fail(
                env,
                ("{}expected file map to contain '{}', " +
                 "but it did not: {}.").format(
                    message_prefix,
                    expected_key,
                    file_map.keys(),
                ),
            )
        elif not file_map[expected_key].endswith(expected_value):
            unittest.fail(
                env,
                ("{}expected file map to have '{}' as '{}', " +
                 "but it did not: {}.").format(
                    message_prefix,
                    expected_value,
                    expected_key,
                    file_map[expected_key],
                ),
            )

    return analysistest.end(env)

def make_output_file_map_test_rule(config_settings = {}):
    """Returns a new `output_file_map_test`-like rule with custom configs.

    Args:
        config_settings: A dictionary of configuration settings and their values
            that should be applied during tests.

    Returns:
        A rule returned by `analysistest.make` that has the
        `output_file_map_test` interface and the given config settings.
    """
    return analysistest.make(
        _output_file_map_test_impl,
        attrs = {
            "expected_mapping": attr.string_dict(
                mandatory = False,
                doc = """\
A dict with the keys and the values expected to be set in the output file map
for the given file entry.
""",
            ),
            "file_entry": attr.string(
                mandatory = True,
                doc = """\
The file entry in the output file map that will be inspected.
""",
            ),
            "output_file_map": attr.string(
                mandatory = True,
                doc = """\
The path of the output file map that is expected to be generated.
The WriteFile action that outputs this file will be inspected.
""",
            ),
        },
        config_settings = config_settings,
    )

# A default instantiation of the rule when no custom config settings are needed.
output_file_map_test = make_output_file_map_test_rule()
