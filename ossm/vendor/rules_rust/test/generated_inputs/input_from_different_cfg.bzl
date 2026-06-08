"""A custom rule that generates a .rs file in a different configuration."""

def _change_cfg_impl(_settings, _attr):
    return {"//test/generated_inputs:change_cfg": True}

change_cfg_transition = transition(
    implementation = _change_cfg_impl,
    inputs = [],
    outputs = ["//test/generated_inputs:change_cfg"],
)

def _input_from_different_cfg_impl(ctx):
    rs_file = ctx.actions.declare_file(ctx.label.name + ".rs")

    ctx.actions.write(
        output = rs_file,
        content = """
pub fn generated_fn() -> String {
    "Generated".to_owned()
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_generated() {
        assert_eq!(super::generated_fn(), "Generated".to_owned());
    }
}
""",
    )

    return OutputGroupInfo(generated_file = depset([rs_file]))

input_from_different_cfg = rule(
    implementation = _input_from_different_cfg_impl,
    attrs = {
        "_allowlist_function_transition": attr.label(
            default = Label("@bazel_tools//tools/allowlists/function_transition_allowlist"),
        ),
    },
    cfg = change_cfg_transition,
)
