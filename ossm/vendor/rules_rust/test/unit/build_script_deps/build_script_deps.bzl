"""Analysis tests for cargo_build_script."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//cargo:defs.bzl", "cargo_build_script")
load("//rust:defs.bzl", "rust_library")

DepActionsInfo = provider(
    "Contains information about dependencies' actions.",
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

def _outputs_contain(outputs, substring):
    for output in outputs.to_list():
        if substring in output.path:
            return True
    return False

def _build_script_deps_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    build_script_deps_action = [
        action
        for action in target[DepActionsInfo].actions
        if _outputs_contain(action.outputs, "dep_of_a_build_script")
    ][0]

    rlib_output = [
        output
        for output in build_script_deps_action.outputs.to_list()
        if output.path.endswith(".rlib")
    ][0]

    asserts.true(env, "-exec-" in rlib_output.path)
    return analysistest.end(env)

build_script_deps_test = analysistest.make(
    _build_script_deps_test_impl,
    extra_target_under_test_aspects = [collect_dep_actions_aspect],
)

def build_script_test_suite(name):
    """Build script analyisis tests.

    Args:
        name: the test suite name
    """
    rust_library(
        name = "dep_of_a_build_script",
        srcs = ["lib.rs"],
        edition = "2021",
    )

    cargo_build_script(
        name = "build_script_deps_in_exec_mode",
        srcs = ["build.rs"],
        deps = [":dep_of_a_build_script"],
        edition = "2021",
    )

    build_script_deps_test(
        name = "build_script_deps_in_exec_mode_test",
        target_under_test = ":build_script_deps_in_exec_mode",
    )

    native.test_suite(
        name = name,
        tests = ["build_script_deps_in_exec_mode_test"],
    )
