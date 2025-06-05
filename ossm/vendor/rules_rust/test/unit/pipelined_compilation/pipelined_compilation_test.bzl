"""Unittests for rust rules."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//rust:defs.bzl", "rust_binary", "rust_library", "rust_proc_macro")
load("//test/unit:common.bzl", "assert_argv_contains", "assert_list_contains_adjacent_elements", "assert_list_contains_adjacent_elements_not")
load(":wrap.bzl", "wrap")

NOT_WINDOWS = select({
    "@platforms//os:linux": [],
    "@platforms//os:macos": [],
    "//conditions:default": ["@platforms//:incompatible"],
})

ENABLE_PIPELINING = {
    str(Label("//rust/settings:pipelined_compilation")): True,
}

def _second_lib_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    rlib_action = [act for act in tut.actions if act.mnemonic == "Rustc"][0]
    metadata_action = [act for act in tut.actions if act.mnemonic == "RustcMetadata"][0]

    # Both actions should use the same --emit=
    assert_argv_contains(env, rlib_action, "--emit=dep-info,link,metadata")
    assert_argv_contains(env, metadata_action, "--emit=dep-info,link,metadata")

    # The metadata action should have a .rmeta as output and the rlib action a .rlib
    path = rlib_action.outputs.to_list()[0].path
    asserts.true(
        env,
        path.endswith(".rlib"),
        "expected Rustc to output .rlib, got " + path,
    )
    path = metadata_action.outputs.to_list()[0].path
    asserts.true(
        env,
        path.endswith(".rmeta"),
        "expected RustcMetadata to output .rmeta, got " + path,
    )

    # Only the action building metadata should contain --rustc-quit-on-rmeta
    assert_list_contains_adjacent_elements_not(env, rlib_action.argv, ["--rustc-quit-on-rmeta", "true"])
    assert_list_contains_adjacent_elements(env, metadata_action.argv, ["--rustc-quit-on-rmeta", "true"])

    # Check that both actions refer to the metadata of :first, not the rlib
    extern_metadata = [arg for arg in metadata_action.argv if arg.startswith("--extern=first=") and "libfirst" in arg and arg.endswith(".rmeta")]
    asserts.true(
        env,
        len(extern_metadata) == 1,
        "did not find a --extern=first=*.rmeta but expected one",
    )
    extern_rlib = [arg for arg in rlib_action.argv if arg.startswith("--extern=first=") and "libfirst" in arg and arg.endswith(".rmeta")]
    asserts.true(
        env,
        len(extern_rlib) == 1,
        "did not find a --extern=first=*.rlib but expected one",
    )

    # Check that the input to both actions is the metadata of :first
    input_metadata = [i for i in metadata_action.inputs.to_list() if i.basename.startswith("libfirst")]
    asserts.true(env, len(input_metadata) == 1, "expected only one libfirst input, found " + str([i.path for i in input_metadata]))
    asserts.true(env, input_metadata[0].extension == "rmeta", "expected libfirst dependency to be rmeta, found " + input_metadata[0].path)
    input_rlib = [i for i in rlib_action.inputs.to_list() if i.basename.startswith("libfirst")]
    asserts.true(env, len(input_rlib) == 1, "expected only one libfirst input, found " + str([i.path for i in input_rlib]))
    asserts.true(env, input_rlib[0].extension == "rmeta", "expected libfirst dependency to be rmeta, found " + input_rlib[0].path)

    return analysistest.end(env)

def _bin_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    bin_action = [act for act in tut.actions if act.mnemonic == "Rustc"][0]

    # Check that no inputs to this binary are .rmeta files.
    metadata_inputs = [i.path for i in bin_action.inputs.to_list() if i.path.endswith(".rmeta")]
    asserts.false(env, metadata_inputs, "expected no metadata inputs, found " + str(metadata_inputs))

    return analysistest.end(env)

bin_test = analysistest.make(_bin_test_impl, config_settings = ENABLE_PIPELINING)
second_lib_test = analysistest.make(_second_lib_test_impl, config_settings = ENABLE_PIPELINING)

def _pipelined_compilation_test():
    rust_proc_macro(
        name = "my_macro",
        edition = "2021",
        srcs = ["my_macro.rs"],
    )

    rust_library(
        name = "first",
        edition = "2021",
        srcs = ["first.rs"],
    )

    rust_library(
        name = "second",
        edition = "2021",
        srcs = ["second.rs"],
        deps = [":first"],
        proc_macro_deps = [":my_macro"],
    )

    rust_binary(
        name = "bin",
        edition = "2021",
        srcs = ["bin.rs"],
        deps = [":second"],
    )

    second_lib_test(name = "second_lib_test", target_under_test = ":second", target_compatible_with = NOT_WINDOWS)
    bin_test(name = "bin_test", target_under_test = ":bin", target_compatible_with = NOT_WINDOWS)

def _rmeta_is_propagated_through_custom_rule_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)

    # This is the metadata-generating action. It should depend on metadata for the library and, if generate_metadata is set
    # also depend on metadata for 'wrapper'.
    rust_action = [act for act in tut.actions if act.mnemonic == "RustcMetadata"][0]

    metadata_inputs = [i for i in rust_action.inputs.to_list() if i.path.endswith(".rmeta")]
    rlib_inputs = [i for i in rust_action.inputs.to_list() if i.path.endswith(".rlib")]

    seen_wrapper_metadata = False
    seen_to_wrap_metadata = False
    for mi in metadata_inputs:
        if "libwrapper" in mi.path:
            seen_wrapper_metadata = True
        if "libto_wrap" in mi.path:
            seen_to_wrap_metadata = True

    seen_wrapper_rlib = False
    seen_to_wrap_rlib = False
    for ri in rlib_inputs:
        if "libwrapper" in ri.path:
            seen_wrapper_rlib = True
        if "libto_wrap" in ri.path:
            seen_to_wrap_rlib = True

    if ctx.attr.generate_metadata:
        asserts.true(env, seen_wrapper_metadata, "expected dependency on metadata for 'wrapper' but not found")
        asserts.false(env, seen_wrapper_rlib, "expected no dependency on object for 'wrapper' but it was found")
    else:
        asserts.true(env, seen_wrapper_rlib, "expected dependency on object for 'wrapper' but not found")
        asserts.false(env, seen_wrapper_metadata, "expected no dependency on metadata for 'wrapper' but it was found")

    asserts.true(env, seen_to_wrap_metadata, "expected dependency on metadata for 'to_wrap' but not found")
    asserts.false(env, seen_to_wrap_rlib, "expected no dependency on object for 'to_wrap' but it was found")

    return analysistest.end(env)

def _rmeta_is_used_when_building_custom_rule_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)

    # This is the custom rule invocation of rustc.
    rust_action = [act for act in tut.actions if act.mnemonic == "Rustc"][0]

    # We want to check that the action depends on metadata, regardless of ctx.attr.generate_metadata
    seen_to_wrap_rlib = False
    seen_to_wrap_rmeta = False
    for act in rust_action.inputs.to_list():
        if "libto_wrap" in act.path and act.path.endswith(".rlib"):
            seen_to_wrap_rlib = True
        elif "libto_wrap" in act.path and act.path.endswith(".rmeta"):
            seen_to_wrap_rmeta = True

    asserts.true(env, seen_to_wrap_rmeta, "expected dependency on metadata for 'to_wrap' but not found")
    asserts.false(env, seen_to_wrap_rlib, "expected no dependency on object for 'to_wrap' but it was found")

    return analysistest.end(env)

rmeta_is_propagated_through_custom_rule_test = analysistest.make(_rmeta_is_propagated_through_custom_rule_test_impl, attrs = {"generate_metadata": attr.bool()}, config_settings = ENABLE_PIPELINING)
rmeta_is_used_when_building_custom_rule_test = analysistest.make(_rmeta_is_used_when_building_custom_rule_test_impl, config_settings = ENABLE_PIPELINING)

def _rmeta_not_produced_if_pipelining_disabled_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)

    rust_action = [act for act in tut.actions if act.mnemonic == "RustcMetadata"]
    asserts.true(env, len(rust_action) == 0, "expected no metadata to be produced, but found a metadata action")

    return analysistest.end(env)

rmeta_not_produced_if_pipelining_disabled_test = analysistest.make(_rmeta_not_produced_if_pipelining_disabled_test_impl, config_settings = ENABLE_PIPELINING)

def _disable_pipelining_test():
    rust_library(
        name = "lib",
        srcs = ["custom_rule_test/to_wrap.rs"],
        edition = "2021",
        disable_pipelining = True,
    )
    rmeta_not_produced_if_pipelining_disabled_test(
        name = "rmeta_not_produced_if_pipelining_disabled_test",
        target_compatible_with = NOT_WINDOWS,
        target_under_test = ":lib",
    )

def _custom_rule_test(generate_metadata, suffix):
    rust_library(
        name = "to_wrap" + suffix,
        crate_name = "to_wrap",
        srcs = ["custom_rule_test/to_wrap.rs"],
        edition = "2021",
    )
    wrap(
        name = "wrapper" + suffix,
        crate_name = "wrapper",
        target = ":to_wrap" + suffix,
        generate_metadata = generate_metadata,
    )
    rust_library(
        name = "uses_wrapper" + suffix,
        srcs = ["custom_rule_test/uses_wrapper.rs"],
        deps = [":wrapper" + suffix],
        edition = "2021",
    )

    rmeta_is_propagated_through_custom_rule_test(
        name = "rmeta_is_propagated_through_custom_rule_test" + suffix,
        generate_metadata = generate_metadata,
        target_compatible_with = NOT_WINDOWS,
        target_under_test = ":uses_wrapper" + suffix,
    )

    rmeta_is_used_when_building_custom_rule_test(
        name = "rmeta_is_used_when_building_custom_rule_test" + suffix,
        target_compatible_with = NOT_WINDOWS,
        target_under_test = ":wrapper" + suffix,
    )

def pipelined_compilation_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _pipelined_compilation_test()
    _disable_pipelining_test()
    _custom_rule_test(generate_metadata = True, suffix = "_with_metadata")
    _custom_rule_test(generate_metadata = False, suffix = "_without_metadata")

    native.test_suite(
        name = name,
        tests = [
            ":bin_test",
            ":second_lib_test",
            ":rmeta_is_propagated_through_custom_rule_test_with_metadata",
            ":rmeta_is_propagated_through_custom_rule_test_without_metadata",
            ":rmeta_is_used_when_building_custom_rule_test_with_metadata",
            ":rmeta_is_used_when_building_custom_rule_test_without_metadata",
            ":rmeta_not_produced_if_pipelining_disabled_test",
        ],
    )
