"""Implementation for jq rule"""

load("//lib:stamping.bzl", "STAMP_ATTRS", "maybe_stamp")
load(":expand_variables.bzl", "expand_variables")
load(":strings.bzl", "split_args")

_jq_attrs = dict({
    "srcs": attr.label_list(
        allow_files = True,
        mandatory = True,
        allow_empty = True,
    ),
    "data": attr.label_list(
        allow_files = True,
    ),
    "filter": attr.string(),
    "filter_file": attr.label(allow_single_file = True),
    "args": attr.string_list(),
    "expand_args": attr.bool(),
    "out": attr.output(),
    "_parse_status_file_filter": attr.label(
        allow_single_file = True,
        default = Label("//lib/private:parse_status_file.jq"),
    ),
}, **STAMP_ATTRS)

def _jq_impl(ctx):
    jq_bin = ctx.toolchains["@aspect_bazel_lib//lib:jq_toolchain_type"].jqinfo.bin

    out = ctx.outputs.out or ctx.actions.declare_file(ctx.attr.name + ".json")
    if ctx.attr.expand_args:
        args = []
        for a in ctx.attr.args:
            args += split_args(expand_variables(ctx, ctx.expand_location(a, targets = ctx.attr.data), outs = [out]))
    else:
        args = ctx.attr.args

    inputs = ctx.files.srcs[:]
    inputs += ctx.files.data

    if not ctx.attr.filter and not ctx.attr.filter_file:
        fail("Must provide a filter or a filter_file")
    if ctx.attr.filter and ctx.attr.filter_file:
        fail("Cannot provide both a filter and a filter_file")

    # jq hangs when there are no input sources unless --null-input flag is passed
    if len(ctx.attr.srcs) == 0 and "-n" not in args and "--null-input" not in args:
        args = args + ["--null-input"]

    if ctx.attr.filter_file:
        args = args + ["--from-file", ctx.file.filter_file.path]
        inputs.append(ctx.file.filter_file)

    stamp = maybe_stamp(ctx)
    if stamp:
        # create an action that gives a JSON representation of the stamp keys
        stamp_json = ctx.actions.declare_file("_%s_stamp.json" % ctx.label.name)
        ctx.actions.run_shell(
            tools = [jq_bin],
            inputs = [stamp.stable_status_file, stamp.volatile_status_file, ctx.file._parse_status_file_filter],
            outputs = [stamp_json],
            command = "{jq} -s -R -f {filter} {stable} {volatile} > {out}".format(
                jq = jq_bin.path,
                filter = ctx.file._parse_status_file_filter.path,
                stable = stamp.stable_status_file.path,
                volatile = stamp.volatile_status_file.path,
                out = stamp_json.path,
            ),
            mnemonic = "ConvertStatusToJson",
            toolchain = "@aspect_bazel_lib//lib:jq_toolchain_type",
        )
        inputs.append(stamp_json)

        args = args + ["--slurpfile", "STAMP", stamp_json.path]

    # quote args that contain spaces
    quoted_args = []
    for a in args:
        if " " in a:
            a = "'{}'".format(a)
        quoted_args.append(a)

    cmd = "{jq} {args} {filter} {sources} > {out}".format(
        jq = jq_bin.path,
        args = " ".join(quoted_args),
        filter = "'%s'" % ctx.attr.filter if ctx.attr.filter else "",
        sources = " ".join(["'%s'" % file.path for file in ctx.files.srcs]),
        out = out.path,
    )

    ctx.actions.run_shell(
        tools = [jq_bin],
        inputs = inputs,
        outputs = [out],
        command = cmd,
        mnemonic = "Jq",
        toolchain = "@aspect_bazel_lib//lib:jq_toolchain_type",
    )

    return DefaultInfo(files = depset([out]), runfiles = ctx.runfiles([out]))

jq_lib = struct(
    attrs = _jq_attrs,
    implementation = _jq_impl,
)
