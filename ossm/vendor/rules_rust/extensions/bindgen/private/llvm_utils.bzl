"""Utilities for llvm targets"""

load("@rules_cc//cc:defs.bzl", "CcInfo")

def _cc_stdcc17_transition_impl(settings, attr):
    # clang --cxxopt=-std=c++17 --host_cxxopt=-std=c++17
    # msvc --cxxopt=/std:c++17 --host_cxxopt=/std:c++17
    host_opt = settings["//command_line_option:host_cxxopt"]
    tgt_opt = settings["//command_line_option:cxxopt"]

    host_opt.extend(attr.cxxopts)
    tgt_opt.extend(attr.cxxopts)

    return {
        "//command_line_option:cxxopt": tgt_opt,
        "//command_line_option:host_cxxopt": host_opt,
    }

_cc_stdcc17_transition = transition(
    implementation = _cc_stdcc17_transition_impl,
    inputs = [
        "//command_line_option:host_cxxopt",
        "//command_line_option:cxxopt",
    ],
    outputs = [
        "//command_line_option:host_cxxopt",
        "//command_line_option:cxxopt",
    ],
)

_CXX_OPTS = select({
    "@platforms//os:windows": ["/std:c++17"],
    "//conditions:default": ["-std=c++17"],
})

_COMMON_ATTRS = {
    "cxxopts": attr.string_list(
        doc = "Flags to inject into `--host_cxxopt` and `--cxxopt` command line flags.",
        mandatory = True,
    ),
    "target": attr.label(
        doc = "The target to transition.",
        mandatory = True,
    ),
}

def _cc_stdcc17_transitioned_target_impl(ctx):
    providers = []

    if CcInfo in ctx.attr.target:
        providers.append(ctx.attr.target[CcInfo])

    if DefaultInfo in ctx.attr.target:
        info = ctx.attr.target[DefaultInfo]
        kwargs = {
            "files": info.files,
            "runfiles": info.default_runfiles,
        }

        if info.files_to_run and info.files_to_run.executable:
            exe = info.files_to_run.executable
            kwargs["executable"] = ctx.actions.declare_file("{}.{}".format(ctx.label.name, exe.extension).rstrip("."))
            ctx.actions.symlink(
                output = kwargs["executable"],
                target_file = exe,
                is_executable = True,
            )

        providers.append(DefaultInfo(**kwargs))

    if OutputGroupInfo in ctx.attr.target:
        providers.append(ctx.attr.target[OutputGroupInfo])

    return providers

_cc_stdcc17_transitioned_library = rule(
    doc = "A rule to transition a C++ library to build with stdc++17.",
    implementation = _cc_stdcc17_transitioned_target_impl,
    cfg = _cc_stdcc17_transition,
    attrs = _COMMON_ATTRS,
)

_cc_stdcc17_transitioned_binary = rule(
    doc = "A rule to transition a C++ binary to build with stdc++17.",
    implementation = _cc_stdcc17_transitioned_target_impl,
    cfg = _cc_stdcc17_transition,
    attrs = _COMMON_ATTRS,
    executable = True,
)

def cc_stdcc17_transitioned_library(name, **kwargs):
    _cc_stdcc17_transitioned_library(
        name = name,
        cxxopts = _CXX_OPTS,
        **kwargs
    )

def cc_stdcc17_transitioned_binary(name, **kwargs):
    _cc_stdcc17_transitioned_binary(
        name = name,
        cxxopts = _CXX_OPTS,
        **kwargs
    )
