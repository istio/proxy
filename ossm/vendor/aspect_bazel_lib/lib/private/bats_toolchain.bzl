"Provide access to a bats executable"

BATS_CORE_VERSIONS = {
    "v1.10.0": "a1a9f7875aa4b6a9480ca384d5865f1ccf1b0b1faead6b47aa47d79709a5c5fd",
}

BATS_SUPPORT_VERSIONS = {
    "v0.3.0": "7815237aafeb42ddcc1b8c698fc5808026d33317d8701d5ec2396e9634e2918f",
}

BATS_ASSERT_VERSIONS = {
    "v2.1.0": "98ca3b685f8b8993e48ec057565e6e2abcc541034ed5b0e81f191505682037fd",
}

BATS_FILE_VERSIONS = {
    "v0.4.0": "9b69043241f3af1c2d251f89b4fcafa5df3f05e97b89db18d7c9bdf5731bb27a",
}

BATS_CORE_TEMPLATE = """\
load("@platforms//host:constraints.bzl", "HOST_CONSTRAINTS")
load("@aspect_bazel_lib//lib/private:bats_toolchain.bzl", "bats_toolchain")
load("@aspect_bazel_lib//lib:copy_to_directory.bzl", "copy_to_directory")

copy_to_directory(
    name = "core",
    hardlink = "on",
    srcs = glob([
        "lib/**",
        "libexec/**"
    ]) + ["bin/bats"],
    out = "bats-core",
)

bats_toolchain(
    name = "toolchain",
    core = ":core",
    libraries = {libraries}
)

toolchain(
    name = "bats_toolchain",
    exec_compatible_with = HOST_CONSTRAINTS,
    toolchain = ":toolchain",
    toolchain_type = "@aspect_bazel_lib//lib:bats_toolchain_type",
)
"""

BATS_LIBRARY_TEMPLATE = """\
load("@aspect_bazel_lib//lib:copy_to_directory.bzl", "copy_to_directory")

copy_to_directory(
    name = "{name}",
    hardlink = "on",
    srcs = glob([
        "src/**",
        "load.bash",
    ]),
    out = "bats-{name}",
    visibility = ["//visibility:public"]
)
"""

BatsInfo = provider(
    doc = "Provide info for executing bats",
    fields = {
        "core": "bats executable",
        "libraries": "bats helper libraries",
    },
)

def _bats_toolchain_impl(ctx):
    core = ctx.file.core

    default_info = DefaultInfo(
        files = depset(ctx.files.core + ctx.files.libraries),
        runfiles = ctx.runfiles(ctx.files.core + ctx.files.libraries),
    )

    batsinfo = BatsInfo(
        core = core,
        libraries = ctx.files.libraries,
    )

    # Export all the providers inside our ToolchainInfo
    # so the resolved_toolchain rule can grab and re-export them.
    toolchain_info = platform_common.ToolchainInfo(
        batsinfo = batsinfo,
        default = default_info,
    )

    return [toolchain_info, default_info]

bats_toolchain = rule(
    implementation = _bats_toolchain_impl,
    attrs = {
        "core": attr.label(
            doc = "Label to the bats executable",
            allow_single_file = True,
            mandatory = True,
        ),
        "libraries": attr.label_list(),
    },
)
