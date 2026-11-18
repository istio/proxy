"Implementation details for sign rule"

_DOC = """Sign an oci_image using cosign binary at a remote registry.

It signs the image by its digest determined beforehand.

```starlark
oci_image(
    name = "image"
)

cosign_sign(
    name = "sign",
    image = ":image",
    repository = "index.docker.io/org/image"
)
```

`repository` attribute can be overridden using the `--repository` flag.

```starlark
oci_image(
    name = "image"
)

cosign_sign(
    name = "sign",
    image = ":image",
    repository = "index.docker.io/org/image"
)
```

run `bazel run :sign -- --repository=index.docker.io/org/test`

You can also omit the `repository` attribute and provide it at runtime.
```starlark
cosign_sign(
    name = "sign_no_repo",
    image = ":image",
)
```
Then run `bazel run :sign_no_repo -- --repository=index.docker.io/org/test`
"""

_attrs = {
    "image": attr.label(allow_single_file = True, mandatory = True, doc = "Label to an oci_image"),
    "repository": attr.string(doc = """        Repository URL where the image will be signed at, e.g.: `index.docker.io/<user>/image`.
        Digests and tags are not allowed. If this attribute is not set, the repository must be passed at runtime via the `--repository` flag.
    """),
    "_sign_sh_tpl": attr.label(default = "sign.sh.tpl", allow_single_file = True),
}

def _cosign_sign_impl(ctx):
    cosign = ctx.toolchains["@rules_oci//cosign:toolchain_type"]
    jq = ctx.toolchains["@aspect_bazel_lib//lib:jq_toolchain_type"]

    if ctx.attr.repository and (ctx.attr.repository.find(":") != -1 or ctx.attr.repository.find("@") != -1):
        fail("repository attribute should not contain digest or tag.")

    executable = ctx.actions.declare_file("cosign_sign_{}.sh".format(ctx.label.name))

    fixed_args = []
    if ctx.attr.repository:
        fixed_args.extend(["--repository", ctx.attr.repository])

    ctx.actions.expand_template(
        template = ctx.file._sign_sh_tpl,
        output = executable,
        is_executable = True,
        substitutions = {
            "{{cosign_path}}": cosign.cosign_info.binary.short_path,
            "{{jq_path}}": jq.jqinfo.bin.short_path,
            "{{image_dir}}": ctx.file.image.short_path,
            "{{fixed_args}}": " ".join(fixed_args),
        },
    )

    runfiles = ctx.runfiles(files = [ctx.file.image])
    runfiles = runfiles.merge(ctx.attr.image[DefaultInfo].default_runfiles)
    runfiles = runfiles.merge(jq.default.default_runfiles)
    runfiles = runfiles.merge(cosign.default.default_runfiles)

    return DefaultInfo(executable = executable, runfiles = runfiles)

cosign_sign = rule(
    implementation = _cosign_sign_impl,
    attrs = _attrs,
    doc = _DOC,
    executable = True,
    toolchains = [
        "@rules_oci//cosign:toolchain_type",
        "@aspect_bazel_lib//lib:jq_toolchain_type",
    ],
)
