"""
Utils for testing js_*() rule outputs
"""

load("@bazel_skylib//rules:build_test.bzl", "build_test")

def _extract_impl(ctx):
    return [
        DefaultInfo(
            files = depset([ctx.file.single]),
        ),
    ]

extract = rule(
    attrs = {
        "single": attr.label(allow_single_file = [".js", ".json"]),
    },
    implementation = _extract_impl,
)

def extract_test(name, target, **kwargs):
    extract(
        name = "_%s_extract" % name,
        single = target,
        **kwargs
    )

    build_test(
        name = name,
        targets = [":_%s_extract" % name],
    )
