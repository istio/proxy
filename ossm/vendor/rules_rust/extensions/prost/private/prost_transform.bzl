"""Prost rules."""

load("@rules_rust//rust:defs.bzl", "rust_common")

ProstTransformInfo = provider(
    doc = "Info about transformations to apply to Prost generated source code.",
    fields = {
        "deps": "List[DepVariantInfo]: Additional dependencies to compile into the Prost target.",
        "prost_opts": "List[str]: Additional prost flags.",
        "srcs": "Depset[File]: Additional source files to include in generated Prost source code.",
        "tonic_opts": "List[str]: Additional tonic flags.",
    },
)

def _rust_prost_transform_impl(ctx):
    deps = []
    for target in ctx.attr.deps:
        deps.append(rust_common.dep_variant_info(
            crate_info = target[rust_common.crate_info] if rust_common.crate_info in target else None,
            dep_info = target[rust_common.dep_info] if rust_common.dep_info in target else None,
            cc_info = target[CcInfo] if CcInfo in target else None,
            build_info = None,
        ))

    # DefaultInfo is intentionally not returned here to avoid impacting other
    # consumers of the `proto_library` target this rule is expected to be passed
    # to.
    return [ProstTransformInfo(
        deps = deps,
        prost_opts = ctx.attr.prost_opts,
        srcs = depset(ctx.files.srcs),
        tonic_opts = ctx.attr.tonic_opts,
    )]

rust_prost_transform = rule(
    doc = """\
A rule for transforming the outputs of `ProstGenProto` actions.

This rule is used by adding it to the `data` attribute of `proto_library` targets. E.g.
```python
load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_rust_prost//:defs.bzl", "rust_prost_library", "rust_prost_transform")

rust_prost_transform(
    name = "a_transform",
    srcs = [
        "a_src.rs",
    ],
)

proto_library(
    name = "a_proto",
    srcs = [
        "a.proto",
    ],
    data = [
        ":transform",
    ],
)

rust_prost_library(
    name = "a_rs_proto",
    proto = ":a_proto",
)
```

The `rust_prost_library` will spawn an action on the `a_proto` target which consumes the
`a_transform` rule to provide a means of granularly modifying a proto library for `ProstGenProto`
actions with minimal impact to other consumers.
""",
    implementation = _rust_prost_transform_impl,
    attrs = {
        "deps": attr.label_list(
            doc = "Additional dependencies to add to the compiled crate.",
            providers = [[rust_common.crate_info], [rust_common.crate_group_info]],
        ),
        "prost_opts": attr.string_list(
            doc = "Additional options to add to Prost.",
        ),
        "srcs": attr.label_list(
            doc = "Additional source files to include in generated Prost source code.",
            allow_files = True,
        ),
        "tonic_opts": attr.string_list(
            doc = "Additional options to add to Tonic.",
        ),
    },
)
