"""Analysis test for for rust_bindgen_library rule."""

load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("@rules_cc//cc:action_names.bzl", "ALL_CPP_COMPILE_ACTION_NAMES")
load("@rules_cc//cc:cc_toolchain_config_lib.bzl", "feature", "flag_group", "flag_set")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_toolchain")
load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_cc//cc/toolchains:cc_toolchain_config_info.bzl", "CcToolchainConfigInfo")
load("@rules_rust//rust:defs.bzl", "rust_binary")
load("@rules_rust_bindgen//:defs.bzl", "rust_bindgen_library")
load("@rules_testing//lib:analysis_test.bzl", "analysis_test", "test_suite")
load("@rules_testing//lib:truth.bzl", "matching")

def _fake_cc_toolchain_config_impl(ctx):
    xclang_flags_feature = feature(
        name = "xclang_flags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = ALL_CPP_COMPILE_ACTION_NAMES,
                flag_groups = [
                    flag_group(flags = [
                        "-Xclang",
                        "-fexperimental-optimized-noescape",
                        "-Xclang",
                        "-fcolor-diagnostics",
                    ]),
                ],
            ),
        ],
    )

    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        toolchain_identifier = "fake-toolchain",
        host_system_name = "unknown",
        target_system_name = "unknown",
        target_cpu = "unknown",
        target_libc = "unknown",
        compiler = "unknown",
        abi_version = "unknown",
        abi_libc_version = "unknown",
        features = [xclang_flags_feature],
    )

_fake_cc_toolchain_config = rule(
    implementation = _fake_cc_toolchain_config_impl,
    attrs = {},
    provides = [CcToolchainConfigInfo],
)

def _fake_cc_toolchain(name):
    _fake_cc_toolchain_config(name = name + "_toolchain_config")

    empty_filegroup = name + "_empty_filegroup"
    native.filegroup(name = empty_filegroup)

    stdbool_file = name + "_stdbool"
    write_file(
        name = stdbool_file,
        content = [],
        out = name + "/resource/dir/include/stdbool.h",
    )

    all_files = name + "_all_files"
    native.filegroup(name = all_files, srcs = [stdbool_file])

    cc_toolchain(
        name = name + "_cc_toolchain",
        toolchain_config = name + "_toolchain_config",
        all_files = all_files,
        dwp_files = empty_filegroup,
        compiler_files = empty_filegroup,
        linker_files = empty_filegroup,
        objcopy_files = empty_filegroup,
        strip_files = empty_filegroup,
    )

    native.toolchain(
        name = name,
        toolchain = name + "_cc_toolchain",
        toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
    )

def _create_simple_rust_bindgen_library(test_name):
    cc_library(
        name = test_name + "_cc",
        hdrs = ["simple.h"],
        srcs = ["simple.cc"],
    )

    rust_bindgen_library(
        name = test_name + "_rust_bindgen",
        cc_lib = test_name + "_cc",
        header = "simple.h",
        tags = ["manual"],
        edition = "2021",
    )

def _test_cc_linkopt_impl(env, target):
    # Assert
    env.expect.that_action(target.actions[0]) \
        .contains_at_least_args(["--codegen=link-arg=-shared"])

def _test_cc_linkopt(name):
    # Arrange
    cc_library(
        name = name + "_cc",
        srcs = ["simple.cc"],
        hdrs = ["simple.h"],
        linkopts = ["-shared"],
        tags = ["manual"],
    )
    rust_bindgen_library(
        name = name + "_rust_bindgen",
        cc_lib = name + "_cc",
        header = "simple.h",
        tags = ["manual"],
        edition = "2021",
    )
    rust_binary(
        name = name + "_rust_binary",
        srcs = ["main.rs"],
        deps = [name + "_rust_bindgen"],
        tags = ["manual"],
        edition = "2021",
    )

    # Act
    # TODO: Use targets attr to also verify `rust_bindgen_library` not having
    # the linkopt after https://github.com/bazelbuild/rules_testing/issues/67
    # is released
    analysis_test(
        name = name,
        target = name + "_rust_binary",
        impl = _test_cc_linkopt_impl,
    )

def _test_cc_lib_object_merging_impl(env, target):
    env.expect.that_int(len(target.actions)).is_greater_than(2)
    env.expect.that_action(target.actions[0]).mnemonic().contains("RustBindgen")
    env.expect.that_action(target.actions[1]).mnemonic().contains("FileWrite")
    env.expect.that_action(target.actions[1]).content().contains("-lstatic=test_cc_lib_object_merging_cc")
    env.expect.that_action(target.actions[2]).mnemonic().contains("FileWrite")
    env.expect.that_action(target.actions[2]).content().contains("-Lnative=")

def _test_cc_lib_object_merging_disabled_impl(env, target):
    # no FileWrite actions writing arg files registered
    env.expect.that_int(len(target.actions)).is_greater_than(0)
    env.expect.that_action(target.actions[0]).mnemonic().contains("RustBindgen")

def _test_cc_lib_object_merging(name):
    cc_library(
        name = name + "_cc",
        hdrs = ["simple.h"],
        srcs = ["simple.cc"],
    )

    rust_bindgen_library(
        name = name + "_rust_bindgen",
        cc_lib = name + "_cc",
        header = "simple.h",
        tags = ["manual"],
        edition = "2021",
    )

    analysis_test(
        name = name,
        target = name + "_rust_bindgen__bindgen",
        impl = _test_cc_lib_object_merging_impl,
    )

def _test_cc_lib_object_merging_disabled(name):
    cc_library(
        name = name + "_cc",
        hdrs = ["simple.h"],
        srcs = ["simple.cc"],
    )

    rust_bindgen_library(
        name = name + "_rust_bindgen",
        cc_lib = name + "_cc",
        header = "simple.h",
        tags = ["manual"],
        merge_cc_lib_objects_into_rlib = False,
        edition = "2021",
    )

    analysis_test(
        name = name,
        target = name + "_rust_bindgen__bindgen",
        impl = _test_cc_lib_object_merging_disabled_impl,
    )

def _test_resource_dir_impl(env, target):
    env.expect.that_int(len(target.actions)).is_greater_than(0)
    env.expect.that_action(target.actions[0]).mnemonic().contains("RustBindgen")
    env.expect.that_action(target.actions[0]).argv().contains_predicate(
        matching.all(
            matching.str_startswith("-resource-dir="),
            matching.str_endswith("/resource/dir"),
        ),
    )

def _test_resource_dir(name):
    _fake_cc_toolchain(name + "_toolchain")

    _create_simple_rust_bindgen_library(name)

    analysis_test(
        name = name,
        target = name + "_rust_bindgen__bindgen",
        impl = _test_resource_dir_impl,
        config_settings = {
            "//command_line_option:extra_toolchains": [str(native.package_relative_label(name + "_toolchain"))],
        },
    )

def _test_strip_xclang_impl(env, target):
    env.expect.that_int(len(target.actions)).is_greater_than(0)
    env.expect.that_action(target.actions[0]).mnemonic().contains("RustBindgen")
    env.expect.that_action(target.actions[0]).not_contains_arg(
        "-fexperimental-optimized-noescape",
    )
    env.expect.that_action(target.actions[0]).contains_at_least_args(
        ["-Xclang", "-fcolor-diagnostics"],
    )

def _test_strip_xclang(name):
    # Test that we strip certain `-Xclang` flags defined by forks of Clang
    # that upstream Clang doesn't know about, such as
    # `-fexperimental-optimized-noescape`. (This is added by the toolchain.)

    _fake_cc_toolchain(name + "_toolchain")

    _create_simple_rust_bindgen_library(name)

    analysis_test(
        name = name,
        target = name + "_rust_bindgen__bindgen",
        impl = _test_strip_xclang_impl,
        config_settings = {
            "//command_line_option:extra_toolchains": [str(native.package_relative_label(name + "_toolchain"))],
        },
    )

def bindgen_test_suite(name):
    test_suite(
        name = name,
        tests = [
            _test_cc_linkopt,
            _test_cc_lib_object_merging,
            _test_cc_lib_object_merging_disabled,
            _test_resource_dir,
            _test_strip_xclang,
        ],
    )
