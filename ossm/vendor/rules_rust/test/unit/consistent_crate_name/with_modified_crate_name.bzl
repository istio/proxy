"""A custom rule that threats all its dependencies as direct dependencies."""

# buildifier: disable=bzl-visibility
load("//rust/private:providers.bzl", "BuildInfo", "CrateInfo", "DepInfo", "DepVariantInfo")

# buildifier: disable=bzl-visibility
load("//rust/private:rustc.bzl", "rustc_compile_action")

def _with_modified_crate_name_impl(ctx):
    toolchain = ctx.toolchains[Label("//rust:toolchain_type")]

    crate_root = ctx.attr.src.files.to_list()[0]
    output_hash = repr(hash(crate_root.path))
    crate_name = ctx.label.name + "_my_custom_crate_suffix"
    crate_type = "rlib"

    rust_lib_name = "{prefix}{name}-{lib_hash}{extension}".format(
        prefix = "lib",
        name = crate_name,
        lib_hash = output_hash,
        extension = ".rlib",
    )

    deps = [DepVariantInfo(
        crate_info = dep[CrateInfo] if CrateInfo in dep else None,
        dep_info = dep[DepInfo] if DepInfo in dep else None,
        build_info = dep[BuildInfo] if BuildInfo in dep else None,
        cc_info = dep[CcInfo] if CcInfo in dep else None,
    ) for dep in ctx.attr.deps]

    rust_lib = ctx.actions.declare_file(rust_lib_name)
    return rustc_compile_action(
        ctx = ctx,
        attr = ctx.attr,
        toolchain = toolchain,
        crate_info_dict = dict(
            name = crate_name,
            type = crate_type,
            root = crate_root,
            srcs = ctx.attr.src.files,
            deps = depset(deps),
            proc_macro_deps = depset([]),
            aliases = {},
            output = rust_lib,
            owner = ctx.label,
            edition = "2018",
            compile_data = depset([]),
            compile_data_targets = depset([]),
            rustc_env = {},
            is_test = False,
        ),
        output_hash = output_hash,
    )

with_modified_crate_name = rule(
    implementation = _with_modified_crate_name_impl,
    attrs = {
        "deps": attr.label_list(),
        "src": attr.label(
            allow_single_file = [".rs"],
        ),
        "_cc_toolchain": attr.label(
            default = Label("@bazel_tools//tools/cpp:current_cc_toolchain"),
        ),
        "_error_format": attr.label(
            default = Label("//rust/settings:error_format"),
        ),
        "_process_wrapper": attr.label(
            default = Label("//util/process_wrapper"),
            executable = True,
            allow_single_file = True,
            cfg = "exec",
        ),
    },
    toolchains = [
        "@rules_rust//rust:toolchain_type",
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
    fragments = ["cpp"],
)
