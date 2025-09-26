"""Internal transition implementations for core Rust rules"""

load("//rust:defs.bzl", "rust_common")

def _import_macro_dep_bootstrap_transition(_settings, _attr):
    """The implementation of the `import_macro_dep_bootstrap_transition` transition.

    This transition modifies the config to start using the fake macro
    implementation, so that the macro itself can be bootstrapped without
    creating a dependency cycle, even while every Rust target has an implicit
    dependency on the "import" macro (either real or fake).

    Args:
        _settings (dict): a dict {String:Object} of all settings declared in the
            inputs parameter to `transition()`.
        _attr (dict): A dict of attributes and values of the rule to which the
            transition is attached.

    Returns:
        dict: A dict of new build settings values to apply.
    """
    return {"@rules_rust//rust/settings:use_real_import_macro": False}

import_macro_dep_bootstrap_transition = transition(
    implementation = _import_macro_dep_bootstrap_transition,
    inputs = [],
    outputs = ["@rules_rust//rust/settings:use_real_import_macro"],
)

def _alias_with_import_macro_bootstrapping_mode_impl(ctx):
    actual = ctx.attr.actual[0]
    return [actual[rust_common.crate_info], actual[rust_common.dep_info]]

alias_with_import_macro_bootstrapping_mode = rule(
    implementation = _alias_with_import_macro_bootstrapping_mode_impl,
    doc = "Alias-like rule to build the `actual` with `use_real_import_macro` setting disabled. Not to be used outside of the import macro bootstrap.",
    attrs = {
        # Using `actual` so tooling such as rust analyzer aspect traverses the target.
        "actual": attr.label(
            doc = "The target this alias refers to.",
            cfg = import_macro_dep_bootstrap_transition,
            mandatory = True,
        ),
        "_allowlist_function_transition": attr.label(
            default = Label("//tools/allowlists/function_transition_allowlist"),
        ),
    },
)
