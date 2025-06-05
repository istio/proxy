"""A custom rule that wraps a crate called to_wrap."""

# buildifier: disable=bzl-visibility
load("//rust/private:providers.bzl", "BuildInfo", "CrateInfo", "DepInfo", "DepVariantInfo")

# buildifier: disable=bzl-visibility
load("//rust/private:rustc.bzl", "rustc_compile_action")

_CONTENT = """\
// crate_name: {}
use to_wrap::to_wrap;

pub fn wrap() {{
    to_wrap();
}}
"""

def _wrap_impl(ctx):
    rs_file = ctx.actions.declare_file(ctx.label.name + "_wrapped.rs")
    crate_name = ctx.attr.crate_name if ctx.attr.crate_name else ctx.label.name
    ctx.actions.write(
        output = rs_file,
        content = _CONTENT.format(crate_name),
    )

    toolchain = ctx.toolchains[Label("//rust:toolchain")]

    # Determine unique hash for this rlib
    output_hash = repr(hash(rs_file.path))
    crate_type = "rlib"

    rust_lib_name = "{prefix}{name}-{lib_hash}{extension}".format(
        prefix = "lib",
        name = crate_name,
        lib_hash = output_hash,
        extension = ".rlib",
    )
    rust_metadata_name = "{prefix}{name}-{lib_hash}{extension}".format(
        prefix = "lib",
        name = crate_name,
        lib_hash = output_hash,
        extension = ".rmeta",
    )

    tgt = ctx.attr.target
    deps = [DepVariantInfo(
        crate_info = tgt[CrateInfo] if CrateInfo in tgt else None,
        dep_info = tgt[DepInfo] if DepInfo in tgt else None,
        build_info = tgt[BuildInfo] if BuildInfo in tgt else None,
        cc_info = tgt[CcInfo] if CcInfo in tgt else None,
    )]

    rust_lib = ctx.actions.declare_file(rust_lib_name)
    rust_metadata = None
    if ctx.attr.generate_metadata:
        rust_metadata = ctx.actions.declare_file(rust_metadata_name)
    return rustc_compile_action(
        ctx = ctx,
        attr = ctx.attr,
        toolchain = toolchain,
        crate_info_dict = dict(
            name = crate_name,
            type = crate_type,
            root = rs_file,
            srcs = depset([rs_file]),
            deps = depset(deps),
            proc_macro_deps = depset([]),
            aliases = {},
            output = rust_lib,
            metadata = rust_metadata,
            owner = ctx.label,
            edition = "2018",
            compile_data = depset([]),
            compile_data_targets = depset([]),
            rustc_env = {},
            is_test = False,
        ),
        output_hash = output_hash,
    )

wrap = rule(
    implementation = _wrap_impl,
    attrs = {
        "crate_name": attr.string(),
        "generate_metadata": attr.bool(default = False),
        "target": attr.label(),
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
    toolchains = ["@rules_rust//rust:toolchain", "@bazel_tools//tools/cpp:toolchain_type"],
    fragments = ["cpp"],
)
