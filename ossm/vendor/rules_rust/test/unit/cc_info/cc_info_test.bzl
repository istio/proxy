"""Unittests for rust rules."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load(
    "@bazel_skylib//rules:common_settings.bzl",
    "BuildSettingInfo",
)
load("@rules_cc//cc:defs.bzl", "cc_import", "cc_library")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load("//rust:defs.bzl", "rust_binary", "rust_common", "rust_library", "rust_proc_macro", "rust_shared_library", "rust_static_library")

def _is_dylib_on_windows(ctx):
    return ctx.target_platform_has_constraint(ctx.attr._windows[platform_common.ConstraintValueInfo])

def _assert_cc_info_has_library_to_link(env, tut, type, ccinfo_count):
    asserts.true(env, CcInfo in tut, "rust_library should provide CcInfo")
    cc_info = tut[CcInfo]
    linker_inputs = cc_info.linking_context.linker_inputs.to_list()
    asserts.equals(env, ccinfo_count, len(linker_inputs))
    library_to_link = linker_inputs[0].libraries[0]
    asserts.equals(env, False, library_to_link.alwayslink)

    asserts.equals(env, [], library_to_link.lto_bitcode_files)
    asserts.equals(env, [], library_to_link.pic_lto_bitcode_files)

    asserts.equals(env, [], library_to_link.objects)
    asserts.equals(env, [], library_to_link.pic_objects)

    if type == "cdylib":
        asserts.true(env, library_to_link.dynamic_library != None)
        if _is_dylib_on_windows(env.ctx):
            asserts.true(env, library_to_link.interface_library != None)
            asserts.true(env, library_to_link.resolved_symlink_dynamic_library == None)
        else:
            asserts.equals(env, None, library_to_link.interface_library)
            asserts.true(env, library_to_link.resolved_symlink_dynamic_library != None)
        asserts.equals(env, None, library_to_link.resolved_symlink_interface_library)
        asserts.equals(env, None, library_to_link.static_library)
        asserts.equals(env, None, library_to_link.pic_static_library)
    else:
        asserts.equals(env, None, library_to_link.dynamic_library)
        asserts.equals(env, None, library_to_link.interface_library)
        asserts.equals(env, None, library_to_link.resolved_symlink_dynamic_library)
        asserts.equals(env, None, library_to_link.resolved_symlink_interface_library)
        asserts.true(env, library_to_link.static_library != None)
        if type in ("rlib", "lib"):
            asserts.true(env, library_to_link.static_library.basename.startswith("lib" + tut.label.name))
        asserts.true(env, library_to_link.pic_static_library != None)
        asserts.equals(env, library_to_link.static_library, library_to_link.pic_static_library)

def _collect_user_link_flags(env, tut):
    asserts.true(env, CcInfo in tut, "rust_library should provide CcInfo")
    cc_info = tut[CcInfo]
    linker_inputs = cc_info.linking_context.linker_inputs.to_list()
    return [f for i in linker_inputs for f in i.user_link_flags]

def _rlib_provides_cc_info_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)

    count = 4
    if ctx.attr._experimental_use_allocator_libraries_with_mangled_symbols[BuildSettingInfo].value:
        count = 3

    _assert_cc_info_has_library_to_link(env, tut, "rlib", count)
    return analysistest.end(env)

def _rlib_with_dep_only_has_stdlib_linkflags_once_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    user_link_flags = _collect_user_link_flags(env, tut)
    asserts.equals(
        env,
        depset(user_link_flags).to_list(),
        user_link_flags,
        "user_link_flags_should_not_have_duplicates_here",
    )
    return analysistest.end(env)

def _bin_does_not_provide_cc_info_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    asserts.false(env, CcInfo in tut, "rust_binary should not provide CcInfo")
    return analysistest.end(env)

def _proc_macro_does_not_provide_cc_info_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    asserts.false(env, CcInfo in tut, "rust_proc_macro should not provide CcInfo")
    return analysistest.end(env)

def _cdylib_provides_cc_info_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    _assert_cc_info_has_library_to_link(env, tut, "cdylib", 2)
    return analysistest.end(env)

def _staticlib_provides_cc_info_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    _assert_cc_info_has_library_to_link(env, tut, "staticlib", 2)
    return analysistest.end(env)

def _crate_group_info_provides_cc_info_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    asserts.true(
        env,
        len(tut[rust_common.dep_info].transitive_noncrates.to_list()) == 1,
        "crate_group_info should provide 1 non-crate transitive dependency",
    )
    return analysistest.end(env)

rlib_provides_cc_info_test = analysistest.make(
    _rlib_provides_cc_info_test_impl,
    attrs = {
        "_experimental_use_allocator_libraries_with_mangled_symbols": attr.label(
            default = Label("//rust/settings:experimental_use_allocator_libraries_with_mangled_symbols"),
        ),
    },
)
rlib_with_dep_only_has_stdlib_linkflags_once_test = analysistest.make(
    _rlib_with_dep_only_has_stdlib_linkflags_once_test_impl,
)
bin_does_not_provide_cc_info_test = analysistest.make(_bin_does_not_provide_cc_info_test_impl)
staticlib_provides_cc_info_test = analysistest.make(_staticlib_provides_cc_info_test_impl)
cdylib_provides_cc_info_test = analysistest.make(_cdylib_provides_cc_info_test_impl, attrs = {
    "_windows": attr.label(default = Label("@platforms//os:windows")),
})
proc_macro_does_not_provide_cc_info_test = analysistest.make(_proc_macro_does_not_provide_cc_info_test_impl)

crate_group_info_provides_cc_info_test = analysistest.make(_crate_group_info_provides_cc_info_test_impl)

def _is_cc_interface_library_test_impl(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)

    cc_info = tut[CcInfo]

    linker_inputs = cc_info.linking_context.linker_inputs.to_list()
    asserts.true(
        env,
        len(linker_inputs) > 0,
        "No linker inputs provided by {}".format(tut.label),
    )

    for linker_input in linker_inputs:
        asserts.true(
            env,
            len(linker_input.libraries) > 0,
            "No linker input libraries provided by {}".format(tut.label),
        )

        for library_to_link in linker_input.libraries:
            asserts.true(
                env,
                library_to_link.dynamic_library == None,
                "dynamic_library unexpectedly provided by {}".format(tut.label),
            )
            asserts.true(
                env,
                library_to_link.static_library == None,
                "static_library unexpectedly provided by {}".format(tut.label),
            )
            asserts.true(
                env,
                library_to_link.pic_static_library == None,
                "pic_static_library unexpectedly provided by {}".format(tut.label),
            )
            asserts.true(
                env,
                library_to_link.interface_library != None,
                "No interface libraries provided by {}".format(tut.label),
            )
    return analysistest.end(env)

is_cc_interface_library_test = analysistest.make(_is_cc_interface_library_test_impl)

def _build_test(ctx):
    env = analysistest.begin(ctx)
    tut = analysistest.target_under_test(env)
    if rust_common.crate_info in tut:
        crate_info = tut[rust_common.crate_info]
    else:
        crate_info = tut[rust_common.test_crate_info].crate
    asserts.true(
        env,
        bool(crate_info.output),
        "No output created by {}".format(tut.label),
    )

    return analysistest.end(env)

build_test = analysistest.make(_build_test)

def _rust_cc_injection_impl(ctx):
    dep_variant_info = rust_common.dep_variant_info(
        cc_info = ctx.attr.cc_dep[CcInfo],
        crate_info = None,
        dep_info = None,
        build_info = None,
    )
    return [
        rust_common.crate_group_info(
            dep_variant_infos = depset([dep_variant_info]),
        ),
    ]

rust_cc_injection = rule(
    attrs = {
        "cc_dep": attr.label(
            providers = [CcInfo],
        ),
    },
    implementation = _rust_cc_injection_impl,
)

def _rust_output_extractor_impl(ctx):
    dep = ctx.attr.dep
    crate_info = dep[rust_common.test_crate_info].crate
    output = crate_info.output

    return [DefaultInfo(
        files = depset([output]),
    )]

rust_output_extractor = rule(
    implementation = _rust_output_extractor_impl,
    attrs = {
        "dep": attr.label(),
    },
)

def _cc_info_test():
    rust_library(
        name = "rlib",
        srcs = ["foo.rs"],
        edition = "2018",
    )

    rust_library(
        name = "rlib_with_dep",
        srcs = ["foo.rs"],
        edition = "2018",
        deps = [":rlib"],
    )

    rust_binary(
        name = "bin",
        srcs = ["foo.rs"],
        edition = "2018",
    )

    rust_static_library(
        name = "staticlib",
        srcs = ["foo.rs"],
        edition = "2018",
    )

    rust_shared_library(
        name = "cdylib",
        srcs = ["foo.rs"],
        edition = "2018",
    )

    rust_proc_macro(
        name = "proc_macro",
        srcs = ["proc_macro.rs"],
        edition = "2018",
        deps = ["//test/unit/native_deps:native_dep"],
    )

    cc_library(
        name = "cc_lib",
        srcs = ["foo.cc"],
    )

    rust_cc_injection(
        name = "cc_lib_injected",
        cc_dep = ":cc_lib",
    )

    rust_output_extractor(
        name = "rust_output_extractor",
        dep = select({
            "@platforms//os:windows": ":staticlib",
            "//conditions:default": ":cdylib",
        }),
    )

    cc_import(
        name = "cc_import",
        interface_library = ":rust_output_extractor",
        system_provided = True,
    )

    rust_library(
        name = "rust_lib_with_cc_lib_injected",
        srcs = ["foo.rs"],
        deps = [":cc_lib_injected"],
        edition = "2018",
    )

    rust_library(
        name = "rust_lib_with_interface_lib_dep",
        srcs = ["foo.rs"],
        deps = [":cc_import"],
        edition = "2018",
    )

    rust_shared_library(
        name = "rust_dylib_with_interface_lib_dep",
        srcs = ["foo.rs"],
        deps = [":cc_import"],
        edition = "2018",
    )

    rlib_provides_cc_info_test(
        name = "rlib_provides_cc_info_test",
        target_under_test = ":rlib",
    )
    rlib_with_dep_only_has_stdlib_linkflags_once_test(
        name = "rlib_with_dep_only_has_stdlib_linkflags_once_test",
        target_under_test = ":rlib_with_dep",
    )
    bin_does_not_provide_cc_info_test(
        name = "bin_does_not_provide_cc_info_test",
        target_under_test = ":bin",
    )
    cdylib_provides_cc_info_test(
        name = "cdylib_provides_cc_info_test",
        target_under_test = ":cdylib",
    )
    staticlib_provides_cc_info_test(
        name = "staticlib_provides_cc_info_test",
        target_under_test = ":staticlib",
    )
    proc_macro_does_not_provide_cc_info_test(
        name = "proc_macro_does_not_provide_cc_info_test",
        target_under_test = ":proc_macro",
    )
    crate_group_info_provides_cc_info_test(
        name = "crate_group_info_provides_cc_info_test",
        target_under_test = ":rust_lib_with_cc_lib_injected",
    )
    is_cc_interface_library_test(
        name = "is_cc_interface_library_test",
        target_under_test = ":cc_import",
    )
    build_test(
        name = "rust_lib_with_interface_lib_dep_test",
        target_under_test = ":rust_lib_with_interface_lib_dep",
    )
    build_test(
        name = "rust_dylib_with_interface_lib_dep_test",
        target_under_test = ":rust_dylib_with_interface_lib_dep",
    )

def cc_info_test_suite(name):
    """Entry-point macro called from the BUILD file.

    Args:
        name: Name of the macro.
    """
    _cc_info_test()

    native.test_suite(
        name = name,
        tests = [
            ":rlib_provides_cc_info_test",
            ":rlib_with_dep_only_has_stdlib_linkflags_once_test",
            ":staticlib_provides_cc_info_test",
            ":cdylib_provides_cc_info_test",
            ":proc_macro_does_not_provide_cc_info_test",
            ":bin_does_not_provide_cc_info_test",
            ":crate_group_info_provides_cc_info_test",
            ":is_cc_interface_library_test",
            ":rust_lib_with_interface_lib_dep_test",
            ":rust_dylib_with_interface_lib_dep_test",
        ],
    )
