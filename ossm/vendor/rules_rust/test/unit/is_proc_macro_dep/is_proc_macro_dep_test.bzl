"""Unittests for the is_proc_macro_dep setting."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//rust:defs.bzl", "rust_library", "rust_proc_macro")

DepActionsInfo = provider(
    "Contains information about dependencies actions.",
    fields = {"actions": "List[Action]"},
)

def _collect_dep_actions_aspect_impl(target, ctx):
    actions = []
    actions.extend(target.actions)
    for dep in ctx.rule.attr.deps:
        actions.extend(dep[DepActionsInfo].actions)
    return [DepActionsInfo(actions = actions)]

collect_dep_actions_aspect = aspect(
    implementation = _collect_dep_actions_aspect_impl,
    attr_aspects = ["deps"],
)

def _attach_dep_actions_aspect_impl(ctx):
    return [ctx.attr.dep[DepActionsInfo]]

attach_dep_actions_aspect = rule(
    implementation = _attach_dep_actions_aspect_impl,
    attrs = {
        "dep": attr.label(aspects = [collect_dep_actions_aspect]),
    },
)

def _enable_is_proc_macro_dep_transition_impl(_settings, _attr):
    return {"//rust/private:is_proc_macro_dep_enabled": True}

enable_is_proc_macro_dep_transition = transition(
    inputs = [],
    outputs = ["//rust/private:is_proc_macro_dep_enabled"],
    implementation = _enable_is_proc_macro_dep_transition_impl,
)

attach_dep_actions_and_enable_is_proc_macro_dep_aspect = rule(
    implementation = _attach_dep_actions_aspect_impl,
    attrs = {
        "dep": attr.label(
            aspects = [collect_dep_actions_aspect],
        ),
        "_allowlist_function_transition": attr.label(
            default = Label(
                "//tools/allowlists/function_transition_allowlist",
            ),
        ),
    },
    cfg = enable_is_proc_macro_dep_transition,
)

def _is_proc_macro_dep_is_not_in_env_for_top_level_action(ctx):
    env = analysistest.begin(ctx)
    top_level_action = analysistest.target_under_test(env).actions[0]
    asserts.false(env, "BAZEL_RULES_RUST_IS_PROC_MACRO_DEP" in top_level_action.env)
    return analysistest.end(env)

is_proc_macro_dep_is_not_in_env_for_top_level_action_test = analysistest.make(_is_proc_macro_dep_is_not_in_env_for_top_level_action)

def _is_proc_macro_dep_is_false_for_proc_macro(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    proc_macro_dep_action = [a for a in tut[DepActionsInfo].actions if str(a) == "action 'Compiling Rust proc-macro proc_macro_crate (1 files)'"][0]
    asserts.equals(env, proc_macro_dep_action.env["BAZEL_RULES_RUST_IS_PROC_MACRO_DEP"], "0")
    return analysistest.end(env)

is_proc_macro_dep_is_false_for_proc_macro_test = analysistest.make(_is_proc_macro_dep_is_false_for_proc_macro)

def _is_proc_macro_dep_is_false_for_top_level_library(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    top_level_library_action = [a for a in tut[DepActionsInfo].actions if str(a) == "action 'Compiling Rust rlib top_level_library (1 files)'"][0]
    asserts.equals(env, top_level_library_action.env["BAZEL_RULES_RUST_IS_PROC_MACRO_DEP"], "0")
    return analysistest.end(env)

is_proc_macro_dep_is_false_for_top_level_library_test = analysistest.make(_is_proc_macro_dep_is_false_for_top_level_library)

def _is_proc_macro_dep_is_true_for_proc_macro_dep(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    proc_macro_dep_action = [a for a in tut[DepActionsInfo].actions if str(a) == "action 'Compiling Rust rlib proc_macro_dep (1 files)'"][0]
    asserts.equals(env, proc_macro_dep_action.env["BAZEL_RULES_RUST_IS_PROC_MACRO_DEP"], "1")
    return analysistest.end(env)

is_proc_macro_dep_is_true_for_proc_macro_dep_test = analysistest.make(_is_proc_macro_dep_is_true_for_proc_macro_dep)

def _is_proc_macro_dep_is_not_in_env_for_proc_macro_dep(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    proc_macro_dep_action = [a for a in tut[DepActionsInfo].actions if str(a) == "action 'Compiling Rust rlib proc_macro_dep (1 files)'"][0]
    asserts.true(env, "BAZEL_RULES_RUST_IS_PROC_MACRO_DEP" not in proc_macro_dep_action.env)
    return analysistest.end(env)

is_proc_macro_dep_is_not_in_env_for_proc_macro_dep_test = analysistest.make(_is_proc_macro_dep_is_not_in_env_for_proc_macro_dep)

def _is_proc_macro_dep_test():
    """Generate targets and tests."""

    rust_library(
        name = "proc_macro_dep",
        srcs = ["proc_macro_dep.rs"],
        edition = "2018",
    )

    rust_proc_macro(
        name = "proc_macro_crate",
        srcs = ["proc_macro_crate.rs"],
        deps = ["proc_macro_dep"],
        edition = "2018",
    )

    is_proc_macro_dep_is_not_in_env_for_top_level_action_test(
        name = "is_proc_macro_dep_is_not_in_env_for_top_level_proc_macro",
        target_under_test = ":proc_macro_crate",
    )

    is_proc_macro_dep_is_not_in_env_for_top_level_action_test(
        name = "is_proc_macro_dep_is_not_in_env_for_top_level_library",
        target_under_test = ":proc_macro_dep",
    )

    attach_dep_actions_and_enable_is_proc_macro_dep_aspect(
        name = "proc_macro_crate_enabled_with_actions",
        dep = ":proc_macro_crate",
    )

    is_proc_macro_dep_is_false_for_proc_macro_test(
        name = "is_proc_macro_dep_is_false_for_proc_macro",
        target_under_test = ":proc_macro_crate_enabled_with_actions",
    )

    is_proc_macro_dep_is_true_for_proc_macro_dep_test(
        name = "is_proc_macro_dep_is_true_for_proc_macro_dep",
        target_under_test = ":proc_macro_crate_enabled_with_actions",
    )

    rust_library(
        name = "top_level_library",
        srcs = ["proc_macro_dep.rs"],
        edition = "2018",
    )

    attach_dep_actions_and_enable_is_proc_macro_dep_aspect(
        name = "top_level_library_enabled_with_actions",
        dep = ":top_level_library",
    )

    is_proc_macro_dep_is_false_for_top_level_library_test(
        name = "is_proc_macro_dep_is_false_for_top_level_library",
        target_under_test = ":top_level_library_enabled_with_actions",
    )

def is_proc_macro_dep_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _is_proc_macro_dep_test()

    native.test_suite(
        name = name,
        tests = [
            "is_proc_macro_dep_is_not_in_env_for_top_level_proc_macro",
            "is_proc_macro_dep_is_not_in_env_for_top_level_library",
            "is_proc_macro_dep_is_false_for_proc_macro",
            "is_proc_macro_dep_is_false_for_top_level_library",
            "is_proc_macro_dep_is_true_for_proc_macro_dep",
        ],
    )
