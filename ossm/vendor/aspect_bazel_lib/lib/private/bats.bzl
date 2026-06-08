"bats_test"

load("//lib:paths.bzl", "BASH_RLOCATION_FUNCTION", "to_rlocation_path")
load("//lib:windows_utils.bzl", "create_windows_native_launcher_script")
load(":expand_variables.bzl", "expand_variables")

_LAUNCHER_TMPL = """#!/usr/bin/env bash
set -o errexit -o nounset -o pipefail

{BASH_RLOCATION_FUNCTION}

readonly core_path="$(rlocation {core})"
readonly bats="$core_path/bin/bats"
readonly libs=( {libraries} )

{envs}

NEW_LIBS=()
for lib in "${{libs[@]}}"; do
    NEW_LIBS+=( $(cd "$(rlocation $lib)/.." && pwd) )
done

export BATS_LIB_PATH=$(
    IFS=:
    echo "${{NEW_LIBS[*]}}"
)
export BATS_TEST_TIMEOUT="$TEST_TIMEOUT"
export BATS_TMPDIR="$TEST_TMPDIR"

# Undocumented: bats can write the JUnit report to any file we specify:
# https://github.com/bats-core/bats-core/blob/b640ec3cf2c7c9cfc9e6351479261186f76eeec8/libexec/bats-core/bats#L400
BATS_REPORT_FILENAME="$(basename $XML_OUTPUT_FILE)" exec $bats {tests} --report-formatter junit --output "$(dirname $XML_OUTPUT_FILE)" "$@"
"""

_ENV_SET = """export {key}=\"{value}\""""

def _bats_test_impl(ctx):
    toolchain = ctx.toolchains["@aspect_bazel_lib//lib:bats_toolchain_type"]
    batsinfo = toolchain.batsinfo
    is_windows = ctx.target_platform_has_constraint(ctx.attr._windows_constraint[platform_common.ConstraintValueInfo])

    envs = []
    for (key, value) in ctx.attr.env.items():
        envs.append(_ENV_SET.format(
            key = key,
            value = expand_variables(ctx, ctx.expand_location(value, targets = ctx.attr.data), attribute_name = "env"),
        ))

    # See https://www.msys2.org/wiki/Porting/:
    # > Setting MSYS2_ARG_CONV_EXCL=* prevents any path transformation.
    if is_windows:
        envs.append(_ENV_SET.format(
            key = "MSYS2_ARG_CONV_EXCL",
            value = "*",
        ))
        envs.append(_ENV_SET.format(
            key = "MSYS_NO_PATHCONV",
            value = "1",
        ))

    bash_launcher = ctx.actions.declare_file("%s_bats.sh" % ctx.label.name)
    ctx.actions.write(
        output = bash_launcher,
        content = _LAUNCHER_TMPL.format(
            core = to_rlocation_path(ctx, batsinfo.core),
            libraries = " ".join([to_rlocation_path(ctx, lib) for lib in batsinfo.libraries]),
            tests = " ".join(["$(rlocation %s)" % to_rlocation_path(ctx, test) for test in ctx.files.srcs]),
            envs = "\n".join(envs),
            BASH_RLOCATION_FUNCTION = BASH_RLOCATION_FUNCTION,
        ),
        is_executable = True,
    )
    launcher = create_windows_native_launcher_script(ctx, bash_launcher) if is_windows else bash_launcher

    runfiles = ctx.runfiles(ctx.files.srcs + ctx.files.data + [bash_launcher])
    runfiles = runfiles.merge(toolchain.default.default_runfiles)
    runfiles = runfiles.merge(ctx.attr._runfiles.default_runfiles)

    return DefaultInfo(
        executable = launcher,
        runfiles = runfiles,
    )

bats_test = rule(
    implementation = _bats_test_impl,
    attrs = {
        "srcs": attr.label_list(
            allow_files = [".bats"],
            doc = "Test files",
        ),
        "data": attr.label_list(
            allow_files = True,
            doc = "Runtime dependencies of the test.",
        ),
        "env": attr.string_dict(
            doc = """Environment variables of the action.

            Subject to [$(location)](https://bazel.build/reference/be/make-variables#predefined_label_variables)
            and ["Make variable"](https://bazel.build/reference/be/make-variables) substitution.
            """,
        ),
        "_runfiles": attr.label(default = "@bazel_tools//tools/bash/runfiles"),
        "_windows_constraint": attr.label(default = "@platforms//os:windows"),
    },
    toolchains = [
        "@aspect_bazel_lib//lib:bats_toolchain_type",
        "@bazel_tools//tools/sh:toolchain_type",
    ],
    test = True,
)
