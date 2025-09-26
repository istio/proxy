"""Implementation for yq rule"""

load("//lib:stamping.bzl", "STAMP_ATTRS", "maybe_stamp")

_yq_attrs = dict({
    "srcs": attr.label_list(
        allow_files = True,
        mandatory = True,
        allow_empty = True,
    ),
    "expression": attr.string(mandatory = False),
    "args": attr.string_list(),
    "outs": attr.output_list(mandatory = True),
    "_parse_status_file_expression": attr.label(
        allow_single_file = True,
        default = Label("//lib/private:parse_status_file.yq"),
    ),
}, **STAMP_ATTRS)

def is_split_operation(args):
    for arg in args:
        if arg.startswith("-s") or arg.startswith("--split-exp"):
            return True
    return False

def _escape_path(path):
    return "/".join([".." for t in path.split("/")]) + "/"

def _yq_impl(ctx):
    yq_bin = ctx.toolchains["@aspect_bazel_lib//lib:yq_toolchain_type"].yqinfo.bin

    outs = ctx.outputs.outs
    args = ctx.attr.args[:]
    inputs = ctx.files.srcs[:]

    split_operation = is_split_operation(args)

    if "eval" in args or "eval-all" in args:
        fail("Do not pass 'eval' or 'eval-all' into yq; this is already set based on the number of srcs")
    if not split_operation and len(outs) > 1:
        fail("Cannot specify multiple outputs when -s or --split-exp is not set")
    if "-i" in args or "--inplace" in args:
        fail("Cannot use arg -i or --inplace as it is not bazel-idiomatic to update the input file; consider using write_source_files to write back to the source tree")
    if len(ctx.attr.srcs) == 0 and "-n" not in args and "--null-input" not in args:
        args = args + ["--null-input"]

    stamp = maybe_stamp(ctx)
    stamp_yaml = ctx.actions.declare_file("_%s_stamp.yaml" % ctx.label.name)
    if stamp:
        # create an action that gives a YAML representation of the stamp keys
        ctx.actions.run_shell(
            tools = [yq_bin],
            inputs = [stamp.stable_status_file, stamp.volatile_status_file, ctx.file._parse_status_file_expression],
            outputs = [stamp_yaml],
            command = "{yq} --from-file {expression} {stable} {volatile} > {out}".format(
                yq = yq_bin.path,
                expression = ctx.file._parse_status_file_expression.path,
                stable = stamp.stable_status_file.path,
                volatile = stamp.volatile_status_file.path,
                out = stamp_yaml.path,
            ),
            mnemonic = "ConvertStatusToYaml",
            toolchain = "@aspect_bazel_lib//lib:yq_toolchain_type",
        )
    else:
        # create an empty stamp file as placeholder
        ctx.actions.write(
            output = stamp_yaml,
            content = "{}",
        )
    inputs.append(stamp_yaml)

    # For split operations, yq outputs files in the same directory so we
    # must cd to the correct output dir before executing it
    bin_dir = ctx.bin_dir.path
    if ctx.label.workspace_name:
        bin_dir = "%s/external/%s" % (bin_dir, ctx.label.workspace_name)
    bin_dir = "/".join([bin_dir, ctx.label.package]) if ctx.label.package else bin_dir
    escape_bin_dir = _escape_path(bin_dir)
    cmd = "cd {bin_dir} && {yq} {args} {eval_cmd} {expression} {sources} {maybe_out}".format(
        bin_dir = bin_dir,
        yq = escape_bin_dir + yq_bin.path,
        eval_cmd = "eval" if len(inputs) <= 1 else "eval-all",
        args = " ".join(args),
        expression = "'%s'" % ctx.attr.expression if ctx.attr.expression else "",
        sources = " ".join(["'%s%s'" % (escape_bin_dir, file.path) for file in ctx.files.srcs]),
        # In the -s/--split-exr case, the out file names are determined by the yq expression
        maybe_out = (" > %s%s" % (escape_bin_dir, outs[0].path)) if len(outs) == 1 else "",
    )

    ctx.actions.run_shell(
        tools = [yq_bin],
        inputs = inputs,
        outputs = outs,
        command = cmd,
        env = {"STAMP": escape_bin_dir + stamp_yaml.path},
        mnemonic = "Yq",
        toolchain = "@aspect_bazel_lib//lib:yq_toolchain_type",
    )

    return DefaultInfo(files = depset(outs), runfiles = ctx.runfiles(outs))

yq_lib = struct(
    attrs = _yq_attrs,
    implementation = _yq_impl,
)
