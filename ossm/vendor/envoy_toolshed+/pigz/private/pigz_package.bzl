"""Build and package the source-built pigz for Linux toolchains.

Only loaded by toolshed's own `//pigz/source` package (dev/bins release).
"""

load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_files", "strip_prefix")
load("@rules_pkg//pkg:pkg.bzl", "pkg_tar")
load("//:versions.bzl", "VERSIONS")

PIGZ_PLATFORMS = {
    "linux-aarch64": "@@toolchains_llvm+//platforms:linux-aarch64",
    "linux-x86_64": "@@toolchains_llvm+//platforms:linux-x86_64",
}

_BUILD_BAZEL = """exports_files(glob(["**"]))
filegroup(name = "pigz", srcs = ["bin/pigz"], visibility = ["//visibility:public"])
"""

def _platform_transition_impl(_settings, attr):
    return {"//command_line_option:platforms": [PIGZ_PLATFORMS[attr.platform]]}

_platform_transition = transition(
    implementation = _platform_transition_impl,
    inputs = [],
    outputs = ["//command_line_option:platforms"],
)

def _run_shell(ctx, inputs, out, command, arguments, mnemonic, tools = []):
    ctx.actions.run_shell(
        inputs = inputs,
        tools = tools,
        outputs = [out],
        command = "mkdir -p $(dirname \"$1\") && " + command,
        arguments = [out.path] + arguments,
        mnemonic = mnemonic,
        progress_message = "Packaging %s" % out.short_path,
    )

def _pigz_files_impl(ctx):
    package_dir = "pigz-%s-%s" % (VERSIONS["pigz"], ctx.attr.platform)
    pigz = ctx.attr.pigz[0][DefaultInfo].files_to_run.executable

    executable = ctx.actions.declare_file(package_dir + "/bin/pigz")
    _run_shell(
        ctx,
        inputs = [pigz],
        tools = [ctx.executable.stripper],
        out = executable,
        command = "\"$2\" -o \"$1\" \"$3\"",
        arguments = [ctx.executable.stripper.path, pigz.path],
        mnemonic = "PigzStrip",
    )

    data = []
    for target, path in ctx.attr.licenses.items():
        src = target.files.to_list()[0]
        out = ctx.actions.declare_file(package_dir + "/share/doc/" + path)
        _run_shell(
            ctx,
            inputs = [src],
            out = out,
            command = "cp -f \"$2\" \"$1\"",
            arguments = [src.path],
            mnemonic = "PigzCopy",
        )
        data.append(out)

    return [
        DefaultInfo(files = depset([executable] + data)),
        OutputGroupInfo(executables = depset([executable]), data = depset(data)),
    ]

_pigz_files = rule(
    implementation = _pigz_files_impl,
    attrs = {
        "licenses": attr.label_keyed_string_dict(
            allow_files = True,
            doc = "License files, keyed to their path under `share/doc/`.",
        ),
        "pigz": attr.label(
            mandatory = True,
            executable = True,
            cfg = _platform_transition,
        ),
        "platform": attr.string(mandatory = True, values = sorted(PIGZ_PLATFORMS.keys())),
        "stripper": attr.label(
            mandatory = True,
            executable = True,
            cfg = "exec",
            allow_single_file = True,
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
)

def pigz_package(name, platform, pigz, licenses, stripper, visibility = None):
    """Build the published pigz tarball for one platform.

    Args:
        name: Macro target name.
        platform: Artifact platform suffix.
        pigz: Source-built `pigz` executable.
        licenses: License files, keyed to their path under `share/doc/`.
        stripper: Executable used to strip the pigz binary.
        visibility: Optional visibility for the final tarball target.
    """
    package_dir = "pigz-%s-%s" % (VERSIONS["pigz"], platform)
    files = name + "_files"
    tarball = name + "_tar"

    native.genrule(
        name = name + "_build",
        outs = [package_dir + "/BUILD.bazel"],
        cmd = """cat >"$@" <<'EOF'
%sEOF""" % _BUILD_BAZEL,
        tags = ["manual"],
    )
    _pigz_files(
        name = files,
        licenses = licenses,
        pigz = pigz,
        platform = platform,
        stripper = stripper,
        tags = ["manual"],
    )
    for group, mode in [("executables", "0755"), ("data", "0644")]:
        native.filegroup(
            name = "%s_%s" % (name, group),
            srcs = [":" + files],
            output_group = group,
            tags = ["manual"],
        )
        pkg_files(
            name = "%s_pkg_%s" % (name, group),
            srcs = [":%s_%s" % (name, group)],
            attributes = pkg_attributes(mode = mode),
            strip_prefix = strip_prefix.from_pkg(package_dir),
            tags = ["manual"],
        )
    pkg_files(
        name = name + "_pkg_build",
        srcs = [":%s_build" % name],
        attributes = pkg_attributes(mode = "0644"),
        strip_prefix = strip_prefix.from_pkg(package_dir),
        tags = ["manual"],
    )
    pkg_tar(
        name = name + "_pkg_tar",
        extension = "tar",
        owner = "0.0",
        package_dir = package_dir,
        package_file_name = package_dir + ".tar",
        srcs = [
            ":%s_pkg_build" % name,
            ":%s_pkg_executables" % name,
            ":%s_pkg_data" % name,
        ],
        tags = ["manual"],
    )
    native.genrule(
        name = tarball,
        srcs = [":%s_pkg_tar" % name],
        outs = [package_dir + ".tar.zst"],
        cmd = "$(location @@aspect_bazel_lib++toolchains+zstd_linux_amd64//:zstd) -19 -T4 -f $(location :%s_pkg_tar) -o $@" % name,
        tools = ["@@aspect_bazel_lib++toolchains+zstd_linux_amd64//:zstd"],
        tags = ["manual"],
    )
    native.filegroup(
        name = name,
        srcs = [":" + tarball],
        tags = ["manual"],
        visibility = visibility,
    )
