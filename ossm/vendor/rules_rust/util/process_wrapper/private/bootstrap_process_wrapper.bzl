"""Bootstrap rustc process wrapper"""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")

def _bootstrap_process_wrapper_impl_unix(ctx):
    output = ctx.actions.declare_file("{}.sh".format(ctx.label.name))

    setting = ctx.attr._use_sh_toolchain_for_bootstrap_process_wrapper[BuildSettingInfo].value
    sh_toolchain = ctx.toolchains["@bazel_tools//tools/sh:toolchain_type"]
    if setting and sh_toolchain:
        shebang = "#!{}".format(sh_toolchain.path)
        ctx.actions.expand_template(
            output = output,
            template = ctx.file._bash,
            substitutions = {
                # Replace the shebang with one constructed from the configured
                # shell toolchain.
                "#!/usr/bin/env bash": shebang,
            },
        )
    else:
        ctx.actions.symlink(
            output = output,
            target_file = ctx.file._bash,
            is_executable = True,
        )

    return [DefaultInfo(
        files = depset([output]),
        executable = output,
    )]

def _bootstrap_process_wrapper_impl_windows(ctx):
    output = ctx.actions.declare_file("{}.bat".format(ctx.label.name))
    ctx.actions.symlink(
        output = output,
        target_file = ctx.file._batch,
        is_executable = True,
    )

    return [DefaultInfo(
        files = depset([output]),
        executable = output,
    )]

def _bootstrap_process_wrapper_impl(ctx):
    if ctx.attr.is_windows:
        return _bootstrap_process_wrapper_impl_windows(ctx)
    return _bootstrap_process_wrapper_impl_unix(ctx)

bootstrap_process_wrapper = rule(
    doc = "A rule which produces a bootstrapping script for the rustc process wrapper.",
    implementation = _bootstrap_process_wrapper_impl,
    attrs = {
        "is_windows": attr.bool(
            doc = "Indicate whether or not the target platform is windows.",
            mandatory = True,
        ),
        "_bash": attr.label(
            allow_single_file = True,
            default = Label("//util/process_wrapper/private:process_wrapper.sh"),
        ),
        "_batch": attr.label(
            allow_single_file = True,
            default = Label("//util/process_wrapper/private:process_wrapper.bat"),
        ),
        "_use_sh_toolchain_for_bootstrap_process_wrapper": attr.label(
            default = Label("//rust/settings:experimental_use_sh_toolchain_for_bootstrap_process_wrapper"),
        ),
    },
    toolchains = [config_common.toolchain_type("@bazel_tools//tools/sh:toolchain_type", mandatory = False)],
    executable = True,
)
