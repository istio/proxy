"""Helper that wraps --proto_compiler into a ProtoLangToolchainInfo for backwards
compatibility with --noincompatible_enable_proto_toolchain_resolution.

Borrowed from https://github.com/bazelbuild/rules_go/pull/3919
"""

load(
    "@rules_proto//proto:proto_common.bzl",
    "ProtoLangToolchainInfo",
)

def _legacy_proto_toolchain_impl(ctx):
    return [
        ProtoLangToolchainInfo(
            proto_compiler = ctx.attr._protoc.files_to_run,
        ),
    ]

legacy_proto_toolchain = rule(
    implementation = _legacy_proto_toolchain_impl,
    attrs = {
        "_protoc": attr.label(
            cfg = "exec",
            default = configuration_field(fragment = "proto", name = "proto_compiler"),
        ),
    },
    fragments = ["proto"],
)
