"""Rules for defining lints to apply to various Rust targets"""

load("//rust/private:providers.bzl", "LintsInfo")

def _rust_lint_config(ctx):
    """Implementation of the `rust_lint_config` rule.

    Args:
        ctx (ctx): The rule's context object.

    Returns:
        list: The LintsInfo provider.
    """

    allowed_levels = ["allow", "warn", "deny", "forbid", "force-warn"]

    rustc_flags = []
    for lint, level in ctx.attr.rustc.items():
        if level not in allowed_levels:
            fail("Invalid rustc lint level '{0}'".format(level))
        rustc_flags.append("--{LEVEL}={LINT}".format(LEVEL = level, LINT = lint))
    for name, values in ctx.attr.rustc_check_cfg.items():
        if len(values) != 0:
            values_list = ", ".join(["\"{0}\"".format(v) for v in values])
            values_arg = ", values({0})".format(values_list)
        else:
            values_arg = ""
        rustc_flags.append("--check-cfg=cfg({NAME}{VALUES})".format(NAME = name, VALUES = values_arg))

    clippy_flags = []
    for lint, level in ctx.attr.clippy.items():
        if level not in allowed_levels:
            fail("Invalid clippy lint level '{0}'".format(level))
        clippy_flags.append("--{LEVEL}=clippy::{LINT}".format(LEVEL = level, LINT = lint))

    rustdoc_flags = []
    for lint, level in ctx.attr.rustdoc.items():
        if level not in allowed_levels:
            fail("Invalid rustdoc lint level '{0}'".format(level))
        rustdoc_flags.append("--{LEVEL}=rustdoc::{LINT}".format(LEVEL = level, LINT = lint))

    return LintsInfo(
        rustc_lint_flags = rustc_flags,
        rustc_lint_files = [],
        clippy_lint_flags = clippy_flags,
        clippy_lint_files = [],
        rustdoc_lint_flags = rustdoc_flags,
        rustdoc_lint_files = [],
    )

# buildifier: disable=unsorted-dict-items
rust_lint_config = rule(
    implementation = _rust_lint_config,
    attrs = {
        "rustc": attr.string_dict(
            doc = "Set of 'rustc' lints to 'allow', 'expect', 'warn', 'force-warn', 'deny', or 'forbid'.",
        ),
        "rustc_check_cfg": attr.string_list_dict(
            doc = "Set of 'cfg' names and list of values to expect.",
        ),
        "clippy": attr.string_dict(
            doc = "Set of 'clippy' lints to 'allow', 'expect', 'warn', 'force-warn', 'deny', or 'forbid'.",
        ),
        "rustdoc": attr.string_dict(
            doc = "Set of 'rustdoc' lints to 'allow', 'expect', 'warn', 'force-warn', 'deny', or 'forbid'.",
        ),
    },
    doc = """\
Defines a group of lints that can be applied when building Rust targets.

For example, you can define a single group of lints:

```python
load("@rules_rust//rust:defs.bzl", "rust_lint_config")

rust_lint_config(
    name = "workspace_lints",
    rustc = {
        "unknown_lints": "allow",
        "unexpected_cfgs": "warn",
    },
    rustc_check_cfg = {
        "bazel": [],
        "fuzzing": [],
        "mz_featutres": ["laser", "rocket"],
    },
    clippy = {
        "box_default": "allow",
        "todo": "warn",
        "unused_async": "warn",
    },
    rustdoc = {
        "unportable_markdown": "allow",
    },
)
```
""",
)
