"""Unit tests for rust toolchains."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("@rules_rust_toolchain_test_target_json//:defs.bzl", "TARGET_JSON")
load("//rust:toolchain.bzl", "rust_stdlib_filegroup", "rust_toolchain")
load("//rust/platform:triple.bzl", "triple")

def _toolchain_specifies_target_triple_test_impl(ctx):
    env = analysistest.begin(ctx)
    toolchain_info = analysistest.target_under_test(env)[platform_common.ToolchainInfo]

    asserts.equals(env, None, toolchain_info.target_json)
    asserts.equals(env, "toolchain-test-triple", toolchain_info.target_flag_value)
    asserts.equals(env, triple("toolchain-test-triple"), toolchain_info.target_triple)
    asserts.equals(env, "toolchain", toolchain_info.target_arch)

    return analysistest.end(env)

def _toolchain_specifies_target_json_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    toolchain_info = target[platform_common.ToolchainInfo]

    asserts.equals(env, None, toolchain_info.target_triple)
    asserts.equals(env, "x86_64", toolchain_info.target_arch)

    # The specific name here is not as vaulable as identifying that `target_json` is a json file
    expected_basename = "{}.target.json".format(target.label.name)
    asserts.equals(env, expected_basename, toolchain_info.target_json.basename)

    # The value is expected to be to a generated file in bazel-out.
    asserts.true(env, toolchain_info.target_flag_value.startswith("bazel-out/"))
    asserts.true(env, toolchain_info.target_flag_value.endswith("/bin/{}/{}".format(ctx.label.package, expected_basename)))

    return analysistest.end(env)

def _toolchain_location_expands_linkflags_impl(ctx):
    env = analysistest.begin(ctx)
    toolchain_info = analysistest.target_under_test(env)[platform_common.ToolchainInfo]

    asserts.equals(
        env,
        "test:test/unit/toolchain/config.txt",
        toolchain_info.stdlib_linkflags.linking_context.linker_inputs.to_list()[0].user_link_flags[0],
    )

    return analysistest.end(env)

def _toolchain_location_expands_extra_rustc_flags_impl(ctx):
    env = analysistest.begin(ctx)
    toolchain_info = analysistest.target_under_test(env)[platform_common.ToolchainInfo]

    asserts.equals(
        env,
        "extra_rustc_flags:test/unit/toolchain/config.txt",
        toolchain_info.extra_rustc_flags[0],
    )

    return analysistest.end(env)

def _std_libs_support_srcs_outside_package_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    actions = analysistest.target_actions(env)

    symlinks = [a for a in actions if a.mnemonic == "Symlink"]
    asserts.equals(env, 2, len(symlinks))

    rlib_symlink = symlinks[0].outputs.to_list()[0]
    asserts.equals(env, tut.label.package + "/core.rlib", rlib_symlink.short_path)

    a_symlink = symlinks[1].outputs.to_list()[0]
    asserts.equals(env, tut.label.package + "/libcore.a", a_symlink.short_path)

    return analysistest.end(env)

toolchain_specifies_target_triple_test = analysistest.make(_toolchain_specifies_target_triple_test_impl)
toolchain_specifies_target_json_test = analysistest.make(_toolchain_specifies_target_json_test_impl)
toolchain_location_expands_linkflags_test = analysistest.make(_toolchain_location_expands_linkflags_impl)
toolchain_location_expands_extra_rustc_flags_test = analysistest.make(_toolchain_location_expands_extra_rustc_flags_impl)
std_libs_support_srcs_outside_package_test = analysistest.make(_std_libs_support_srcs_outside_package_test_impl)

def _define_test_targets():
    native.filegroup(
        name = "stdlib_srcs",
        srcs = ["config.txt"],
    )
    rust_stdlib_filegroup(
        name = "std_libs",
        srcs = [":stdlib_srcs"],
    )

    rust_stdlib_filegroup(
        name = "std_libs_with_srcs_outside_package",
        srcs = ["//test/unit/toolchain/subpackage:std_libs_srcs"],
    )

    native.filegroup(
        name = "target_json",
        srcs = ["toolchain-test-triple.json"],
    )

    write_file(
        name = "mock_rustc",
        out = "mock_rustc.exe",
        content = [],
        is_executable = True,
    )

    write_file(
        name = "mock_rustdoc",
        out = "mock_rustdoc.exe",
        content = [],
        is_executable = True,
    )

    rust_toolchain(
        name = "rust_triple_toolchain",
        binary_ext = "",
        dylib_ext = ".so",
        exec_triple = "x86_64-unknown-none",
        rust_doc = ":mock_rustdoc",
        rust_std = ":std_libs",
        rustc = ":mock_rustc",
        staticlib_ext = ".a",
        stdlib_linkflags = [],
        target_triple = "toolchain-test-triple",
    )

    encoded_target_json = json.encode(TARGET_JSON)

    rust_toolchain(
        name = "rust_json_toolchain",
        binary_ext = "",
        dylib_ext = ".so",
        exec_triple = "x86_64-unknown-none",
        rust_doc = ":mock_rustdoc",
        rust_std = ":std_libs",
        rustc = ":mock_rustc",
        staticlib_ext = ".a",
        stdlib_linkflags = [],
        target_json = encoded_target_json,
    )

    rust_toolchain(
        name = "rust_inline_json_toolchain",
        binary_ext = "",
        dylib_ext = ".so",
        exec_triple = "x86_64-unknown-none",
        rust_doc = ":mock_rustdoc",
        rust_std = ":std_libs",
        rustc = ":mock_rustc",
        staticlib_ext = ".a",
        stdlib_linkflags = [],
        target_json = json.encode(
            {
                "arch": "x86_64",
                "env": "gnu",
                "llvm-target": "x86_64-unknown-linux-gnu",
                "target-family": ["unix"],
                "target-pointer-width": "64",
            },
        ),
    )

    rust_toolchain(
        name = "rust_location_expand_toolchain",
        binary_ext = "",
        dylib_ext = ".so",
        exec_triple = "x86_64-unknown-none",
        rust_doc = ":mock_rustdoc",
        rust_std = ":std_libs",
        rustc = ":mock_rustc",
        staticlib_ext = ".a",
        stdlib_linkflags = ["test:$(location :stdlib_srcs)"],
        extra_rustc_flags = ["extra_rustc_flags:$(location :stdlib_srcs)"],
        target_json = encoded_target_json,
    )

def toolchain_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name (str): The name of the test suite.
    """
    _define_test_targets()

    toolchain_specifies_target_triple_test(
        name = "toolchain_specifies_target_triple_test",
        target_under_test = ":rust_triple_toolchain",
    )
    toolchain_specifies_target_json_test(
        name = "toolchain_specifies_target_json_test",
        target_under_test = ":rust_json_toolchain",
    )
    toolchain_specifies_target_json_test(
        name = "toolchain_specifies_inline_target_json_test",
        target_under_test = ":rust_inline_json_toolchain",
    )
    toolchain_location_expands_linkflags_test(
        name = "toolchain_location_expands_linkflags_test",
        target_under_test = ":rust_location_expand_toolchain",
    )
    toolchain_location_expands_extra_rustc_flags_test(
        name = "toolchain_location_expands_extra_rustc_flags_test",
        target_under_test = ":rust_location_expand_toolchain",
    )
    std_libs_support_srcs_outside_package_test(
        name = "std_libs_support_srcs_outside_package_test",
        target_under_test = ":std_libs_with_srcs_outside_package",
    )

    native.test_suite(
        name = name,
        tests = [
            ":toolchain_specifies_target_triple_test",
            ":toolchain_specifies_target_json_test",
            ":toolchain_specifies_inline_target_json_test",
            ":toolchain_location_expands_linkflags_test",
            ":toolchain_location_expands_extra_rustc_flags_test",
            ":std_libs_support_srcs_outside_package_test",
        ],
    )
