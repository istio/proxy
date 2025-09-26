"""Unittests for the --experimental_use_cc_common_link build setting."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("@rules_cc//cc:defs.bzl", "cc_library")
load(
    "@rules_rust//rust:defs.bzl",
    "rust_binary",
    "rust_shared_library",
    "rust_test",
)

DepActionsInfo = provider(
    "Contains information about dependencies actions.",
    fields = {"actions": "List[Action]"},
)

def _collect_dep_actions_aspect_impl(target, _ctx):
    return [DepActionsInfo(actions = target.actions)]

collect_dep_actions_aspect = aspect(
    implementation = _collect_dep_actions_aspect_impl,
)

def _use_cc_common_link_transition_impl(_settings, _attr):
    return {"@rules_rust//rust/settings:experimental_use_cc_common_link": True}

use_cc_common_link_transition = transition(
    inputs = [],
    outputs = ["@rules_rust//rust/settings:experimental_use_cc_common_link"],
    implementation = _use_cc_common_link_transition_impl,
)

def _use_cc_common_link_on_target_impl(ctx):
    return [ctx.attr.target[0][DepActionsInfo]]

use_cc_common_link_on_target = rule(
    implementation = _use_cc_common_link_on_target_impl,
    attrs = {
        "target": attr.label(
            cfg = use_cc_common_link_transition,
            aspects = [collect_dep_actions_aspect],
        ),
        "_allowlist_function_transition": attr.label(
            default = Label("@bazel_tools//tools/allowlists/function_transition_allowlist"),
        ),
    },
)

def _outputs_object_file(action):
    object_files = [output for output in action.outputs.to_list() if output.extension in ("o", "obj")]
    return len(object_files) > 0

def _use_cc_common_link_test(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    registered_actions = tut[DepActionsInfo].actions

    # When --experimental_use_cc_common_link is enabled the compile+link Rustc action produces a
    # .o/.obj file.
    rustc_action = [action for action in registered_actions if action.mnemonic == "Rustc"][0]
    asserts.true(env, _outputs_object_file(rustc_action), "Rustc action did not output an object file")

    has_cpp_link_action = len([action for action in registered_actions if action.mnemonic == "CppLink"]) > 0
    asserts.true(env, has_cpp_link_action, "Expected that the target registers a CppLink action")

    return analysistest.end(env)

use_cc_common_link_test = analysistest.make(_use_cc_common_link_test)

def _custom_malloc_test(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    registered_actions = tut[DepActionsInfo].actions
    links = [action for action in registered_actions if action.mnemonic == "CppLink"]
    cmdline = " ".join(links[0].argv)
    asserts.true(env, "this_library_is_not_really_an_allocator" in cmdline, "expected to find custom malloc in linker invocation")
    return analysistest.end(env)

custom_malloc_test = analysistest.make(
    _custom_malloc_test,
    config_settings = {
        "//command_line_option:custom_malloc": "@//unit:this_library_is_not_really_an_allocator",
    },
)

def _cc_common_link_test_targets():
    """Generate targets and tests."""

    cc_library(
        name = "this_library_is_not_really_an_allocator",
        srcs = ["this_library_is_not_really_an_allocator.c"],
    )

    rust_binary(
        name = "bin",
        srcs = ["bin.rs"],
        edition = "2018",
    )

    use_cc_common_link_on_target(
        name = "bin_with_cc_common_link",
        target = ":bin",
    )

    rust_shared_library(
        name = "cdylib",
        srcs = ["lib.rs"],
        edition = "2018",
    )

    use_cc_common_link_on_target(
        name = "cdylib_with_cc_common_link",
        target = ":cdylib",
    )

    rust_test(
        name = "test_with_srcs",
        srcs = ["lib.rs"],
        edition = "2018",
    )

    use_cc_common_link_on_target(
        name = "test_with_cc_common_link",
        target = ":test_with_srcs",
        testonly = True,
    )

    rust_test(
        name = "test-with-crate",
        crate = "cdylib",
        edition = "2018",
    )

    use_cc_common_link_on_target(
        name = "crate_test_with_cc_common_link",
        target = ":test-with-crate",
        testonly = True,
    )

    use_cc_common_link_test(
        name = "use_cc_common_link_on_binary",
        target_under_test = ":bin_with_cc_common_link",
    )

    use_cc_common_link_test(
        name = "use_cc_common_link_on_test",
        target_under_test = ":test_with_cc_common_link",
    )

    use_cc_common_link_test(
        name = "use_cc_common_link_on_crate_test",
        target_under_test = ":crate_test_with_cc_common_link",
    )

    use_cc_common_link_test(
        name = "use_cc_common_link_on_cdylib",
        target_under_test = ":cdylib_with_cc_common_link",
    )

    custom_malloc_test(
        name = "custom_malloc_on_binary_test",
        target_under_test = ":bin_with_cc_common_link",
    )

def cc_common_link_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _cc_common_link_test_targets()

    native.test_suite(
        name = name,
        tests = [
            "use_cc_common_link_on_binary",
            "use_cc_common_link_on_test",
            "use_cc_common_link_on_crate_test",
            "use_cc_common_link_on_cdylib",
            "custom_malloc_on_binary_test",
        ],
    )
