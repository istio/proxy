"Implementation details for oci_image_index rule"

_DOC = """Build a multi-architecture OCI compatible container image.

It takes number of `oci_image` targets to create a fat multi-architecture image conforming to [OCI Image Index Specification](https://github.com/opencontainers/image-spec/blob/main/image-index.md).

Image indexes can be created in two ways:

## Using Bazel platforms

While this feature is still experimental, it is the recommended way to create image indexes.

```starlark
go_binary(
    name = "app_can_cross_compile"
)

tar(
    name = "app_layer",
    srcs = [
        ":app_can_cross_compile",
    ],
)

oci_image(
    name = "image",
    tars = [":app_layer"],
)

oci_image_index(
    name = "image_multiarch",
    images = [":image"],
    platforms = [
        "@rules_go//go/toolchain:linux_amd64",
        "@rules_go//go/toolchain:linux_arm64",
    ],
)
```

## Without using Bazel platforms

```starlark
oci_image(
    name = "app_linux_amd64"
)

oci_image(
    name = "app_linux_arm64"
)

oci_image_index(
    name = "app",
    images = [
        ":app_linux_amd64",
        ":app_linux_arm64"
    ]
)
```
"""

def _image_index_transition_impl(_, attr):
    return [
        {"//command_line_option:platforms": str(platform)}
        for platform in attr.platforms
    ]

_image_index_transition = transition(
    implementation = _image_index_transition_impl,
    inputs = [],
    outputs = ["//command_line_option:platforms"],
)

_attrs = {
    "images": attr.label_list(
        mandatory = True,
        doc = "List of labels to oci_image targets.",
        cfg = _image_index_transition,
    ),
    "platforms": attr.label_list(
        doc = """This feature is highly EXPERIMENTAL and not subject to our usual SemVer guarantees.
A list of platform targets to build the image for. If specified, only one image can be specified in the images attribute.
""",
        providers = [platform_common.PlatformInfo],
    ),
    "annotations": attr.label(
        doc = "A file containing a dictionary of annotations to apply to the index manifest. Each line should be in the form `name=value`.",
        allow_single_file = True,
    ),
    "_image_index_sh_tpl": attr.label(default = "image_index.sh.tpl", allow_single_file = True),
    "_allowlist_function_transition": attr.label(
        default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
    ),
}

def _expand_image_to_args(image, expander):
    args = [
        "--image={}".format(image.path),
    ]
    for file in expander.expand(image):
        if file.path.find("blobs") != -1:
            args.append("--blob={}".format(file.tree_relative_path))
    return args

def _oci_image_index_impl(ctx):
    if len(ctx.attr.platforms) > 0 and len(ctx.attr.images) != len(ctx.attr.platforms):
        fail("platforms can only be specified when there is exactly one image in the images attribute.")

    jq = ctx.toolchains["@aspect_bazel_lib//lib:jq_toolchain_type"]
    coreutils = ctx.toolchains["@aspect_bazel_lib//lib:coreutils_toolchain_type"]

    launcher = ctx.actions.declare_file("image_index_{}.sh".format(ctx.label.name))
    ctx.actions.expand_template(
        template = ctx.file._image_index_sh_tpl,
        output = launcher,
        is_executable = True,
        substitutions = {
            "{{jq_path}}": jq.jqinfo.bin.path,
            "{{coreutils_path}}": coreutils.coreutils_info.bin.path,
        },
    )

    output = ctx.actions.declare_directory(ctx.label.name)

    args = ctx.actions.args()
    args.add(output.path, format = "--output=%s")
    args.add_all(ctx.files.images, map_each = _expand_image_to_args, expand_directories = False)

    inputs = list(ctx.files.images)
    if ctx.attr.annotations:
        args.add(ctx.file.annotations.path, format = "--annotations=%s")
        inputs.append(ctx.file.annotations)

    ctx.actions.run(
        inputs = inputs,
        arguments = [args],
        outputs = [output],
        executable = launcher,
        tools = [jq.jqinfo.bin, coreutils.coreutils_info.bin],
        mnemonic = "OCIIndex",
        progress_message = "OCI Index %{label}",
        toolchain = None,
    )

    return DefaultInfo(files = depset([output]))

oci_image_index = rule(
    implementation = _oci_image_index_impl,
    attrs = _attrs,
    doc = _DOC,
    toolchains = [
        "@aspect_bazel_lib//lib:jq_toolchain_type",
        "@aspect_bazel_lib//lib:coreutils_toolchain_type",
    ],
)
