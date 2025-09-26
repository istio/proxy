"""Aspect and rules for generating SPDX documents."""

load("//devtools/compliance/spdx/generator:fmt.bzl", "info_to_json")
load("//tools/build_defs/license:gather_licenses_info.bzl", "gather_licenses_info")
load("//tools/build_defs/license/private:gathering_providers.bzl", "TransitiveLicensesInfo")
load("//third_party/bazel_skylib/rules:common_settings.bzl", "BuildSettingInfo")

def _collect_inputs(info):
    licenses = []
    metadatas = []
    for license in info.licenses.to_list():
        if hasattr(license, "metadata") and license.metadata and type(license.metadata) == "File":
            metadatas.append(license.metadata)
        if license.license_text and type(license.license_text) == "File":
            licenses.append(license.license_text)
    return licenses + metadatas

def _spdx_common(ctx, target, spdx_output, _gen_spdx_tool):
    name = "%s_info.json" % ctx.label.name
    aspect_output = ctx.actions.declare_file(name)

    # ... possibly traverse output from aspect and assemble it for writing...
    info = target[TransitiveLicensesInfo]

    # If the result doesn't contain licenses, we simply return the provider
    if not hasattr(info, "target_under_license"):
        return [OutputGroupInfo()]

    content = "[\n%s\n]\n" % ",\n".join(info_to_json(info))

    # write aspect output
    ctx.actions.write(output = aspect_output, content = content)

    crass_id = None
    if hasattr(ctx.attr, "_copyright_attribution_id"):
        if BuildSettingInfo in ctx.attr._copyright_attribution_id:
            crass_id = ctx.attr._copyright_attribution_id[BuildSettingInfo].value

    args = ctx.actions.args()
    args.add("--output", spdx_output)
    args.add("--input", aspect_output)
    args.add("--build_changelist", ctx.version_file)
    args.add("--target", target.label)
    if crass_id:
        args.add("--copyright_attribution_id", crass_id)

    inputs = depset([aspect_output, ctx.version_file] + _collect_inputs(info))

    # postprocess into spdx output
    ctx.actions.run(
        inputs = inputs,
        outputs = [spdx_output],
        arguments = [args],
        progress_message = "Assembling SPDX for %s" % target,
        executable = _gen_spdx_tool,
    )
    return [
        OutputGroupInfo(
            files_propagated_to_buildrabbit_INTERNAL_ = depset([spdx_output]),
            spdx = depset([spdx_output]),
        ),
    ]

def _spdx_document_impl(ctx):
    for target in ctx.attr.deps:
        spdx_output = ctx.outputs.out
        _spdx_common(ctx, target, spdx_output, ctx.executable._gen_spdx)

spdx_document = rule(
    implementation = _spdx_document_impl,
    attrs = {
        "deps": attr.label_list(
            aspects = [gather_licenses_info],
        ),
        "out": attr.output(
            mandatory = True,
        ),
        "_gen_spdx": attr.label(
            default = "//devtools/compliance/spdx/generator:gen_spdx_tool",
            executable = True,
            cfg = "exec",
        ),
        # Defined by flag --//devtools/compliance/spdx/generator:copyright_attribution_id=<id>
        "_copyright_attribution_id": attr.label(default = ":copyright_attribution_id"),
    },
)

def _gen_spdx_impl(target, ctx):
    spdx_output = ctx.actions.declare_file("%s.spdx.json" % ctx.label.name)
    return _spdx_common(ctx, target, spdx_output, ctx.executable._gen_spdx)

gen_spdx = aspect(
    implementation = _gen_spdx_impl,
    requires = [gather_licenses_info],
    attrs = {
        "_gen_spdx": attr.label(
            default = "//devtools/compliance/spdx/generator:gen_spdx_tool",
            executable = True,
            cfg = "exec",
        ),
        # Defined by flag --//devtools/compliance/spdx/generator:copyright_attribution_id=<id>
        "_copyright_attribution_id": attr.label(default = ":copyright_attribution_id"),
    },
)

