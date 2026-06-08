""" Define a custom transition that sets the pip_whl flag to no """

def _flag_transition_impl(_settings, _ctx):
    return {"//python/config_settings:pip_whl": "no"}

flag_transition = transition(
    implementation = _flag_transition_impl,
    inputs = [],
    outputs = ["//python/config_settings:pip_whl"],
)

# Define a rule that applies the transition to dependencies
def _transition_rule_impl(_ctx):
    return [DefaultInfo()]

transition_rule = rule(
    implementation = _transition_rule_impl,
    attrs = {
        "deps": attr.label_list(cfg = flag_transition),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
)
