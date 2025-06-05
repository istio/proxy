"""Analysis tests for experimental_link_std_dylib flag"""

load("@rules_cc//cc:defs.bzl", "CcInfo")
load("@rules_rust//rust:defs.bzl", "rust_binary", "rust_library")
load("@rules_testing//lib:analysis_test.bzl", "analysis_test", "test_suite")

# buildifier: disable=bzl-visibility
load("//rust/private:utils.bzl", "is_std_dylib")

def _test_rust_binary_impl(env, targets):
    env.expect.that_action(targets.default_binary.actions[0]) \
        .contains_none_of_flag_values([
        ("--codegen", "prefer-dynamic"),
    ])

    # Make sure with @rules_rust//rust/settings:experimental_link_std_dylib,
    # the linker flags are set up correct so that the binary dynamically links
    # the stdlib
    env.expect.that_action(targets.binary_with_std_dylib.actions[0]) \
        .contains_flag_values([
        ("--codegen", "prefer-dynamic"),
    ])

def _test_rust_binary(name):
    rust_binary(
        name = name + "_rust_binary",
        srcs = ["main.rs"],
        edition = "2021",
        tags = ["manual"],
    )

    analysis_test(
        name = name,
        impl = _test_rust_binary_impl,
        targets = {
            "binary_with_std_dylib": name + "_rust_binary",
            "default_binary": name + "_rust_binary",
        },
        attrs = {
            "binary_with_std_dylib": {
                "@config_settings": {
                    str(Label("@rules_rust//rust/settings:experimental_link_std_dylib")): True,
                },
            },
        },
    )

def _export_static_stdlibs_in_cc_info(target):
    linker_inputs = target[CcInfo].linking_context.linker_inputs
    for linker_input in linker_inputs.to_list():
        for library in linker_input.libraries:
            if hasattr(library, "pic_static_library") and library.pic_static_library != None:
                basename = library.pic_static_library.basename
                if basename.startswith("libstd") and basename.endswith(".a"):
                    return True
    return False

def _export_libstd_dylib_in_cc_info(target):
    linker_inputs = target[CcInfo].linking_context.linker_inputs
    for linker_input in linker_inputs.to_list():
        for library in linker_input.libraries:
            if hasattr(library, "dynamic_library") and library.dynamic_library != None:
                if is_std_dylib(library.dynamic_library):
                    return True
    return False

def _test_rust_library_impl(env, targets):
    # By default, rust_library exports static stdlibs to downstream shared
    # and binary targets to statically link
    env.expect \
        .that_bool(_export_static_stdlibs_in_cc_info(targets.default_rlib)) \
        .equals(True)
    env.expect \
        .that_bool(_export_libstd_dylib_in_cc_info(targets.default_rlib)) \
        .equals(False)

    # With @rules_rust//rust/settings:experimental_link_std_dylib
    # rust_library exports dylib std and does not export static stdlibs to
    # downstream shared and binary targets to dynamically link
    env.expect \
        .that_bool(_export_static_stdlibs_in_cc_info(targets.rlib_with_std_dylib)) \
        .equals(False)
    env.expect \
        .that_bool(_export_libstd_dylib_in_cc_info(targets.rlib_with_std_dylib)) \
        .equals(True)

def _test_rust_library(name):
    rust_library(
        name = name + "_rust_library",
        srcs = ["lib.rs"],
        edition = "2021",
        tags = ["manual"],
    )

    analysis_test(
        name = name,
        impl = _test_rust_library_impl,
        targets = {
            "default_rlib": name + "_rust_library",
            "rlib_with_std_dylib": name + "_rust_library",
        },
        attrs = {
            "rlib_with_std_dylib": {
                "@config_settings": {
                    str(Label("@rules_rust//rust/settings:experimental_link_std_dylib")): True,
                },
            },
        },
    )

def link_std_dylib_test_suite(name):
    test_suite(
        name = name,
        tests = [
            _test_rust_binary,
            _test_rust_library,
        ],
    )
