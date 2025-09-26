"expand_template rule"

load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("//lib:stamping.bzl", "STAMP_ATTRS", "maybe_stamp")
load(":expand_variables.bzl", "expand_variables")

def _expand_substitutions(ctx, output, substitutions):
    result = {}
    for k, v in substitutions.items():
        result[k] = expand_variables(ctx, ctx.expand_location(v, targets = ctx.attr.data), outs = [output], attribute_name = "substitutions")
    return result

def _expand_template_impl(ctx):
    output = ctx.outputs.out
    if not output:
        if ctx.file.template and ctx.file.template.is_source:
            output = ctx.actions.declare_file(ctx.file.template.basename, sibling = ctx.file.template)
        else:
            output = ctx.actions.declare_file(ctx.attr.name + ".txt")

    substitutions = _expand_substitutions(ctx, output, ctx.attr.substitutions)
    expand_template_info = ctx.toolchains["@aspect_bazel_lib//lib:expand_template_toolchain_type"].expand_template_info
    stamp = maybe_stamp(ctx)
    if stamp:
        substitutions = dicts.add(substitutions, _expand_substitutions(ctx, output, ctx.attr.stamp_substitutions))
        substitutions_out = ctx.actions.declare_file("{}_substitutions.json".format(ctx.label.name))
        ctx.actions.write(
            output = substitutions_out,
            content = json.encode(substitutions),
        )

        inputs = [
            ctx.file.template,
            stamp.volatile_status_file,
            stamp.stable_status_file,
            substitutions_out,
        ]

        args = ctx.actions.args()
        args.add(ctx.file.template)
        args.add(output)
        args.add(substitutions_out)
        args.add(stamp.volatile_status_file)
        args.add(stamp.stable_status_file)
        args.add(ctx.attr.is_executable)

        ctx.actions.run(
            arguments = [args],
            outputs = [output],
            inputs = inputs,
            executable = expand_template_info.bin,
            toolchain = "@aspect_bazel_lib//lib:expand_template_toolchain_type",
        )
    else:
        ctx.actions.expand_template(
            template = ctx.file.template,
            output = output,
            substitutions = substitutions,
            is_executable = ctx.attr.is_executable,
        )

    all_outs = [output]
    runfiles = ctx.runfiles(files = all_outs)
    return [DefaultInfo(files = depset(all_outs), runfiles = runfiles)]

expand_template_lib = struct(
    doc = """Template expansion

This performs a simple search over the template file for the keys in substitutions,
and replaces them with the corresponding values.

Values may also use location templates as documented in
[expand_locations](https://github.com/bazel-contrib/bazel-lib/blob/main/docs/expand_make_vars.md#expand_locations)
as well as [configuration variables](https://docs.bazel.build/versions/main/skylark/lib/ctx.html#var)
such as `$(BINDIR)`, `$(TARGET_CPU)`, and `$(COMPILATION_MODE)` as documented in
[expand_variables](https://github.com/bazel-contrib/bazel-lib/blob/main/docs/expand_make_vars.md#expand_variables).
""",
    implementation = _expand_template_impl,
    toolchains = ["@aspect_bazel_lib//lib:expand_template_toolchain_type"],
    attrs = dicts.add({
        "data": attr.label_list(
            doc = "List of targets for additional lookup information.",
            allow_files = True,
        ),
        "is_executable": attr.bool(
            doc = "Whether to mark the output file as executable.",
        ),
        "out": attr.output(
            doc = """Where to write the expanded file.

            If the `template` is a source file, then `out` defaults to
            be named the same as the template file and outputted to the same
            workspace-relative path. In this case there will be no pre-declared
            label for the output file. It can be referenced by the target label
            instead. This pattern is similar to `copy_to_bin` but with substitutions on
            the copy.

            Otherwise, `out` defaults to `[name].txt`.
            """,
        ),
        "stamp_substitutions": attr.string_dict(
            doc = """Mapping of strings to substitutions.

            There are overlaid on top of substitutions when stamping is enabled
            for the target.

            Substitutions can contain $(execpath :target) and $(rootpath :target)
            expansions, $(MAKEVAR) expansions and {{STAMP_VAR}} expansions when
            stamping is enabled for the target.
            """,
        ),
        "substitutions": attr.string_dict(
            doc = """Mapping of strings to substitutions.

            Substitutions can contain $(execpath :target) and $(rootpath :target)
            expansions, $(MAKEVAR) expansions and {{STAMP_VAR}} expansions when
            stamping is enabled for the target.
            """,
        ),
        "template": attr.label(
            doc = "The template file to expand.",
            mandatory = True,
            allow_single_file = True,
        ),
    }, **STAMP_ATTRS),
)

expand_template = rule(
    doc = expand_template_lib.doc,
    implementation = expand_template_lib.implementation,
    toolchains = expand_template_lib.toolchains,
    attrs = expand_template_lib.attrs,
)
