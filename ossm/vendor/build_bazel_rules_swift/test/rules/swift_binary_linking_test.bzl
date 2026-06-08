"""Rules for testing the swift_binary's linking output."""

load("@bazel_skylib//lib:collections.bzl", "collections")
load("@bazel_skylib//lib:unittest.bzl", "analysistest", "unittest")

def _swift_binary_linking_test_impl(ctx):
    env = analysistest.begin(ctx)
    target_under_test = analysistest.target_under_test(env)

    # Find the Linking action that outputs binary.
    actions = analysistest.target_actions(env)
    output_binary_path = ctx.attr.output_binary_path

    action_outputs = [
        (action, [file.short_path for file in action.outputs.to_list()])
        for action in actions
    ]
    matching_actions = [
        action
        for action, outputs in action_outputs
        if output_binary_path in outputs
    ]
    if not matching_actions:
        actual_outputs = collections.uniq([
            output.short_path
            for action in actions
            for output in action.outputs.to_list()
        ])
        unittest.fail(
            env,
            ("Target '{}' registered no actions that outputs '{}' " +
             "(it had {}).").format(
                str(target_under_test.label),
                output_binary_path,
                actual_outputs,
            ),
        )
        return analysistest.end(env)
    if len(matching_actions) != 1:
        unittest.fail(
            env,
            ("Expected exactly one action that outputs '{}', " +
             "but found {}.").format(
                output_binary_path,
                len(matching_actions),
            ),
        )
        return analysistest.end(env)

    return analysistest.end(env)

def make_swift_binary_linking_test_rule(config_settings = {}):
    """Returns a new `swift_binary_linking_test`-like rule with custom configs

    Args:
        config_settings: A dictionary of configuration settings and their values
            that should be applied during tests.

    Returns:
        a rule returned by `analysistest.make` that has the
        `swift_binary_linking_test` interface and the given config settings
    """
    return analysistest.make(
        _swift_binary_linking_test_impl,
        attrs = {
            "output_binary_path": attr.string(
                mandatory = True,
                doc = "Expected path of generated binary.",
            ),
        },
        config_settings = config_settings,
    )

# A default instantiation of the rule when no custom config settings are needed.
swift_binary_linking_test = make_swift_binary_linking_test_rule()
