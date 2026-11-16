"""Edit the system files of an OCI base image."""

_JQ_TOOLCHAIN = "@aspect_bazel_lib//lib:jq_toolchain_type"
_TAR_TOOLCHAIN = "@aspect_bazel_lib//lib:tar_toolchain_type"

def _base_edit_impl(ctx):
    jq = ctx.toolchains[_JQ_TOOLCHAIN].jqinfo.bin
    tar_toolchain = ctx.toolchains[_TAR_TOOLCHAIN]
    tar = tar_toolchain.tarinfo.binary
    spec = ctx.actions.declare_file("%s.spec.json" % ctx.label.name)
    ctx.actions.write(spec, ctx.attr.spec)
    out = ctx.actions.declare_file("%s.tar" % ctx.label.name)
    # The image layout: a directory, or the files of one (with `index.json` at its root).
    images = ctx.files.image
    if len(images) == 1 and images[0].is_directory:
        layout = images[0].path
    else:
        index = [f for f in images if f.basename == "index.json"]
        if len(index) != 1:
            fail("`image` must be an OCI image layout directory, or its files including `index.json`")
        layout = index[0].dirname
    args = ctx.actions.args()
    args.add(layout)
    args.add(spec)
    args.add(out)
    args.add_all(ctx.files.packages)
    env = dict(tar_toolchain.tarinfo.default_env)
    env["JQ"] = jq.path
    env["TAR"] = tar.path
    ctx.actions.run(
        executable = ctx.executable._script,
        arguments = [args],
        inputs = images + [spec] + ctx.files.packages,
        tools = depset(
            [jq],
            transitive = [tar_toolchain.default.files],
        ),
        outputs = [out],
        env = env,
        mnemonic = "OCIBaseEdit",
        progress_message = "Editing base image system files %{label}",
    )
    return [DefaultInfo(files = depset([out]))]

base_edit = rule(
    implementation = _base_edit_impl,
    attrs = {
        "image": attr.label(
            doc = "The base image: an OCI image layout for a single platform (eg from `oci_pull`), as a directory, or its files.",
            allow_files = True,
            mandatory = True,
        ),
        "packages": attr.label_list(
            doc = "Debian package control archives (eg rules_distroless `@<repo>//<package>:control`) to add to the dpkg status.",
            allow_files = True,
        ),
        "spec": attr.string(
            doc = "The users and groups to add, as JSON (see `oci_base_edit`).",
            mandatory = True,
        ),
        "_script": attr.label(
            default = Label("//oci/private:base_edit.sh"),
            allow_single_file = True,
            executable = True,
            cfg = "exec",
        ),
    },
    toolchains = [
        _JQ_TOOLCHAIN,
        _TAR_TOOLCHAIN,
    ],
)
