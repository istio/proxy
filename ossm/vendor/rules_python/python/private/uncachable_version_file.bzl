"""Implementation of uncachable_version_file."""

load(":py_internal.bzl", "py_internal")

def _uncachable_version_file_impl(ctx):
    version_file = ctx.actions.declare_file("uncachable_version_file.txt")
    py_internal.copy_without_caching(
        ctx = ctx,
        # NOTE: ctx.version_file is undocumented; see
        # https://github.com/bazelbuild/bazel/issues/9363
        # NOTE: Even though the version file changes every build (it contains
        # the build timestamp), it is ignored when computing what inputs
        # changed. See https://bazel.build/docs/user-manual#workspace-status
        read_from = ctx.version_file,
        write_to = version_file,
    )
    return [DefaultInfo(
        files = depset([version_file]),
    )]

uncachable_version_file = rule(
    doc = """
Creates a copy of `ctx.version_file`, except it isn't ignored by
Bazel's change-detecting logic. In fact, it's the opposite:
caching is disabled for the action generating this file, so any
actions depending on this file will always re-run.
""",
    implementation = _uncachable_version_file_impl,
)

def define_uncachable_version_file(name):
    native.alias(
        name = name,
        actual = select({
            ":stamp_detect": ":uncachable_version_file_impl",
            "//conditions:default": ":sentinel",
        }),
    )
    uncachable_version_file(name = "uncachable_version_file_impl")
