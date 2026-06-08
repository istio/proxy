"""Unittests for the --experimental_use_cc_common_link build setting."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("@rules_cc//cc:defs.bzl", "cc_library")
load(
    "@rules_rust//rust:defs.bzl",
    "rust_binary",
    "rust_shared_library",
    "rust_test",
)
load("@rules_rust//rust:toolchain.bzl", "rust_stdlib_filegroup", "rust_toolchain")

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
    return [ctx.attr.target[0][DepActionsInfo], ctx.attr.target[0][OutputGroupInfo]]

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

def _with_collect_dep_actions_impl(ctx):
    # return [ctx.attr.target[DepActionsInfo], ctx.attr.target[OutputGroupInfo]]
    return [ctx.attr.target[DepActionsInfo]]

with_collect_dep_actions = rule(
    implementation = _with_collect_dep_actions_impl,
    attrs = {
        "target": attr.label(
            aspects = [collect_dep_actions_aspect],
        ),
    },
)

def _with_exec_cfg_impl(ctx):
    return [ctx.attr.target[DepActionsInfo]]

with_exec_cfg = rule(
    implementation = _with_exec_cfg_impl,
    attrs = {
        "target": attr.label(
            cfg = "exec",
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

    output_groups = tut[OutputGroupInfo]
    asserts.false(env, hasattr(output_groups, "dsym_folder"), "Expected no dsym_folder output group")
    asserts.equals(
        env,
        ctx.attr.expect_pdb,
        hasattr(output_groups, "pdb_file"),
        "Expected " + ("" if ctx.attr.expect_pdb else "no ") + "pdb_file output group",
    )

    return analysistest.end(env)

use_cc_common_link_test = analysistest.make(_use_cc_common_link_test, attrs = {"expect_pdb": attr.bool()})

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
        "//command_line_option:custom_malloc": str(Label("@//unit:this_library_is_not_really_an_allocator")),
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

    rust_binary(
        name = "bin_with_pdb",
        srcs = ["bin.rs"],
        edition = "2018",
        features = ["generate_pdb_file"],
    )

    use_cc_common_link_on_target(
        name = "bin_with_cc_common_link_with_pdb",
        target = ":bin_with_pdb",
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
        name = "use_cc_common_link_on_binary_with_pdb",
        target_under_test = ":bin_with_cc_common_link_with_pdb",
        expect_pdb = select({
            "@platforms//os:windows": True,
            "//conditions:default": False,
        }),
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

    return [
        "use_cc_common_link_on_binary",
        "use_cc_common_link_on_binary_with_pdb",
        "use_cc_common_link_on_test",
        "use_cc_common_link_on_crate_test",
        "use_cc_common_link_on_cdylib",
        "custom_malloc_on_binary_test",
    ]

_RUSTC_FLAGS_CODEGEN_UNITS = 2
_SETTINGS_CODEGEN_UNITS = 3
_PER_CRATE_RUSTC_FLAG_CODEGEN_UNITS = 4
_EXTRA_RUSTC_FLAG_CODEGEN_UNITS = 5
_EXTRA_RUSTC_FLAGS_CODEGEN_UNITS = 6
_EXTRA_EXEC_RUSTC_FLAG_CODEGEN_UNITS = 7
_EXTRA_EXEC_RUSTC_FLAGS_CODEGEN_UNITS = 8
_TOOLCHAIN_EXTRA_RUSTC_FLAGS_CODEGEN_UNITS = 9
_TOOLCHAIN_EXTRA_EXEC_RUSTC_FLAGS_CODEGEN_UNITS = 10
_TOOLCHAIN_EXTRA_RUSTC_FLAGS_FOR_CRATE_TYPES = 11

def _codegen_units_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    registered_actions = tut[DepActionsInfo].actions

    rustc_actions = [action for action in registered_actions if action.mnemonic == "Rustc"]
    asserts.equals(env, 1, len(rustc_actions))
    rustc_action = rustc_actions[0]

    actual = sorted([arg for arg in rustc_action.argv if arg.startswith("-Ccodegen-units=")])
    expected = sorted(["-Ccodegen-units=%s" % codegen_units for codegen_units in ctx.attr.expected_codegen_units])

    asserts.equals(env, expected, actual)

    return analysistest.end(env)

codegen_units_test = analysistest.make(
    _codegen_units_test_impl,
    config_settings = {
        # By default, don't use cc_common.link to link.
        str(Label("@rules_rust//rust/settings:experimental_use_cc_common_link")): False,
        # Set `-Ccodegen-units` in various different ways.
        str(Label("@rules_rust//rust/settings:codegen_units")): _SETTINGS_CODEGEN_UNITS,
        # Empty prefix (before the `@`) means the flag is applied to all crates.
        str(Label("@rules_rust//rust/settings:experimental_per_crate_rustc_flag")): ["@-Ccodegen-units=%s" % _PER_CRATE_RUSTC_FLAG_CODEGEN_UNITS],
        str(Label("@rules_rust//rust/settings:extra_rustc_flag")): ["-Ccodegen-units=%s" % _EXTRA_RUSTC_FLAG_CODEGEN_UNITS],
        str(Label("@rules_rust//rust/settings:extra_rustc_flags")): ["-Ccodegen-units=%s" % _EXTRA_RUSTC_FLAGS_CODEGEN_UNITS],
        str(Label("@rules_rust//rust/settings:extra_exec_rustc_flag")): ["-Ccodegen-units=%s" % _EXTRA_EXEC_RUSTC_FLAG_CODEGEN_UNITS],
        str(Label("@rules_rust//rust/settings:extra_exec_rustc_flags")): ["-Ccodegen-units=%s" % _EXTRA_EXEC_RUSTC_FLAGS_CODEGEN_UNITS],
        "//command_line_option:extra_toolchains": ["//unit:codegen_units_toolchain"],
    },
    attrs = {"expected_codegen_units": attr.int_list()},
)

def _codegen_units_test_targets():
    # Targets under test.
    rust_binary(
        name = "codegen_units_bin",
        srcs = ["bin.rs"],
        edition = "2018",
        rustc_flags = ["-Ccodegen-units=%s" % _RUSTC_FLAGS_CODEGEN_UNITS],
    )
    use_cc_common_link_on_target(
        name = "codegen_units_bin_with_cc_common_link",
        target = ":codegen_units_bin",
    )
    with_collect_dep_actions(
        name = "codegen_units_bin_with_collect_dep_actions",
        target = ":codegen_units_bin",
    )
    with_exec_cfg(
        name = "codegen_units_exec_bin_with_cc_common_link",
        target = ":codegen_units_bin_with_cc_common_link",
    )
    with_exec_cfg(
        name = "codegen_units_exec_bin_with_collect_dep_actions",
        target = ":codegen_units_bin_with_collect_dep_actions",
    )

    # Fake toolchain along with its dependencies.
    write_file(
        name = "mock_rustc",
        out = "mock_rustc.exe",
    )
    write_file(
        name = "stdlib_src_self_contained",
        out = "self-contained/something.o",
    )
    rust_stdlib_filegroup(
        name = "std_libs",
        srcs = ["self-contained/something.o"],
    )
    write_file(
        name = "mock_rustdoc",
        out = "mock_rustdoc.exe",
    )
    write_file(
        name = "mock_rust-lld",
        out = "mock_rust-lld.exe",
    )
    rust_toolchain(
        name = "codegen_units_toolchain_impl",
        binary_ext = "",
        dylib_ext = ".so",
        exec_triple = "x86_64-unknown-none",
        target_triple = "x86_64-unknown-none",
        rust_doc = ":mock_rustdoc",
        rust_std = ":std_libs",
        rustc = ":mock_rustc",
        linker = ":mock_rust-lld",
        linker_type = "direct",
        staticlib_ext = ".a",
        stdlib_linkflags = [],
        extra_rustc_flags = ["-Ccodegen-units=%s" % _TOOLCHAIN_EXTRA_RUSTC_FLAGS_CODEGEN_UNITS],
        extra_exec_rustc_flags = ["-Ccodegen-units=%s" % _TOOLCHAIN_EXTRA_EXEC_RUSTC_FLAGS_CODEGEN_UNITS],
        extra_rustc_flags_for_crate_types = {"bin": ["-Ccodegen-units=%s" % _TOOLCHAIN_EXTRA_RUSTC_FLAGS_FOR_CRATE_TYPES]},
        visibility = ["//visibility:public"],
    )
    native.toolchain(
        name = "codegen_units_toolchain",
        toolchain = ":codegen_units_toolchain_impl",
        toolchain_type = "@rules_rust//rust:toolchain",
    )

    # Tests
    codegen_units_test(
        name = "codegen_units_filtered",
        target_under_test = ":codegen_units_bin_with_cc_common_link",
        # When using cc_common.link, we expect an explicit `-Ccodegen-units=1`.
        expected_codegen_units = [1],
    )
    codegen_units_test(
        name = "codegen_units_filtered_exec",
        target_under_test = ":codegen_units_exec_bin_with_cc_common_link",
        # When using cc_common.link, we expect an explicit `-Ccodegen-units=1`.
        expected_codegen_units = [1],
    )
    codegen_units_test(
        name = "codegen_units_not_filtered",
        target_under_test = ":codegen_units_bin_with_collect_dep_actions",
        # When not using cc_common.link, all `-Ccodegen-units` flags added in
        # various ways should remain unchanged.
        expected_codegen_units = [
            _RUSTC_FLAGS_CODEGEN_UNITS,
            _SETTINGS_CODEGEN_UNITS,
            _PER_CRATE_RUSTC_FLAG_CODEGEN_UNITS,
            _EXTRA_RUSTC_FLAG_CODEGEN_UNITS,
            _EXTRA_RUSTC_FLAGS_CODEGEN_UNITS,
            _TOOLCHAIN_EXTRA_RUSTC_FLAGS_CODEGEN_UNITS,
            _TOOLCHAIN_EXTRA_RUSTC_FLAGS_FOR_CRATE_TYPES,
        ],
    )
    codegen_units_test(
        name = "codegen_units_not_filtered_exec",
        target_under_test = ":codegen_units_exec_bin_with_collect_dep_actions",
        # When not using cc_common.link, all `-Ccodegen-units` flags added in
        # various ways should remain unchanged.
        expected_codegen_units = [
            _RUSTC_FLAGS_CODEGEN_UNITS,
            _SETTINGS_CODEGEN_UNITS,
            _EXTRA_EXEC_RUSTC_FLAG_CODEGEN_UNITS,
            _EXTRA_EXEC_RUSTC_FLAGS_CODEGEN_UNITS,
            _TOOLCHAIN_EXTRA_EXEC_RUSTC_FLAGS_CODEGEN_UNITS,
            _TOOLCHAIN_EXTRA_RUSTC_FLAGS_FOR_CRATE_TYPES,
        ],
    )

    return [
        "codegen_units_filtered",
        "codegen_units_filtered_exec",
        "codegen_units_not_filtered",
        "codegen_units_not_filtered_exec",
    ]

def cc_common_link_test_suite(name, **kwargs):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
        **kwargs: Additional keyword arguments,
    """
    native.test_suite(
        name = name,
        tests =
            _cc_common_link_test_targets() +
            _codegen_units_test_targets(),
        **kwargs
    )
