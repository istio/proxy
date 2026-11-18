"repos for buildx"

load("@aspect_bazel_lib//lib:repo_utils.bzl", "repo_utils")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")

BUILDX_WORKAROUND = """
def _oci_layout_buildx_workaround_impl(ctx):
    '''workaround for buildx to accept oci-layout with lock file and ingest directory already created
    See: https://github.com/docker/buildx/issues/2753https://github.com/docker/buildx/issues/2753#issuecomment-2436601290https://github.com/containerd/containerd/issues/10885https://github.com/docker/buildx/issues/2753#issuecomment-2436324404
    '''
    output = ctx.actions.declare_directory(ctx.label.name)
    coreutils = ctx.toolchains["@aspect_bazel_lib//lib:coreutils_toolchain_type"].coreutils_info.bin
    ctx.actions.run_shell(
        outputs = [output],
        inputs = [ctx.file.layout],
        tools = [coreutils],
        toolchain = "@aspect_bazel_lib//lib:coreutils_toolchain_type",
        command = '''
for blob in $($COREUTILS ls -1 -d "$LAYOUT/blobs/"*/*); do
    relative_to_blobs="${blob#"$LAYOUT/blobs"}"
    $COREUTILS mkdir -p "$OUTPUT/blobs/$($COREUTILS dirname "$relative_to_blobs")"
    # Relative path from `output/blobs/sha256/` to `$blob`
    relative="$($COREUTILS realpath --relative-to="$OUTPUT/blobs/sha256" "$blob" --no-symlinks)"
    $COREUTILS ln -s "$relative" "$OUTPUT/blobs/$relative_to_blobs"
done
$COREUTILS cp --no-preserve=mode "$LAYOUT/oci-layout" "$OUTPUT/oci-layout"
$COREUTILS cp --no-preserve=mode "$LAYOUT/index.json" "$OUTPUT/index.json"
$COREUTILS touch "$OUTPUT/index.json.lock"
$COREUTILS mkdir "$OUTPUT/ingest"
$COREUTILS touch "$OUTPUT/ingest/.keep"
        ''',
        env = {
            "COREUTILS": coreutils.path,
            "OUTPUT": output.path,
            "LAYOUT": ctx.file.layout.path,
        },
        mnemonic = "WorkaroundBuildX",
    )
    return DefaultInfo(
        files = depset([output]),
        runfiles = ctx.attr.layout[DefaultInfo].default_runfiles,
    )

oci_layout_buildx_workaround = rule(
    implementation = _oci_layout_buildx_workaround_impl,
    attrs = {
        "layout": attr.label(allow_single_file = True),
    },
    toolchains = ["@aspect_bazel_lib//lib:coreutils_toolchain_type"],
)
"""

BUILDX_RULE = """
def buildx(name, dockerfile, path = ".", srcs = [], build_context = [], execution_requirements = {"local": "1"}, tags = ["manual"], visibility = []):
    \"\"\"
    Run BuildX to produce OCI base image using a Dockerfile. 

    Args:
        name: name of the target
        dockerfile: label to the dockerfile to use for this build
        path: path to build context where all will be relative to under Dockerfile
        build_context: a dictionary for custom build contexes. See https://docs.docker.com/reference/cli/docker/buildx/build/#build-context
        execution_requirements: execution requirements for the action, we recommend using local as BuildX wants to read files outside of the sandbox.
        tags: tags for the target
        visibility: visibility for the target
    \"\"\"
    if "requires-docker" not in tags:
        tags = tags + ["requires-docker"]

    context_args = []
    context_srcs = []
    for context in build_context:
        context_srcs = context_srcs + context["srcs"]
        context_args.append("--build-context={}={}".format(context["replace"], context["store"]))
    

    copy_file(
        name = name + "_dockerfile",
        src = dockerfile,
        out = "Dockerfile." + name,
    )


    run_binary(
        name = name,
        srcs = [name + "_dockerfile"] + srcs + context_srcs,
        args = [
            "build",
            path,
            "--file",
            "$(location {}_dockerfile)".format(name),
            "--builder",
            BUILDER_NAME,
            "--output=type=oci,tar=false,dest=$@",
            # Set the source date epoch to 0 for better reproducibility.
            "--build-arg SOURCE_DATE_EPOCH=0",
        ] + context_args,
        execution_requirements =  execution_requirements,
        mnemonic = "BuildX",
        out_dirs = [name],
        target_compatible_with = TARGET_COMPATIBLE_WITH,
        tool = "@buildx//:buildx",
        tags = tags,
        visibility = visibility,
    )

"""

BUILDX_CONTEXT_SYNTAX_SUGAR = """
def context_oci_layout(replace, layout):
    name = str(replace).replace("/", "_").replace(":", "_")
    # TODO: remove once https://github.com/docker/buildx/issues/2753 is solved and set store to layout directly.
    oci_layout_buildx_workaround(
        name = name,
        layout = layout,
    )
    return {
        "replace": replace,
        "store": "oci-layout://$(location %s)" % name,
        "srcs": [name],
    }


def context_sources(replace, sources, override_path = None):
    store = "$(location %s)" % sources
    if override_path:
        store = override_path
        
    return {
        "replace": replace,
        "store": store,
        "srcs": sources,
    }

context = struct(
    oci_layout = context_oci_layout,
    sources = context_sources
)
"""

def _impl_configure_buildx(rctx):
    has_docker = False

    # See if standard docker sock exists
    if not has_docker:
        r = rctx.execute(["stat", "/var/run/docker.sock"])
        if r.return_code == 0:
            has_docker = True

    compatible_with = "[]"
    builder_name = "builder-docker"
    if has_docker:
        buildx = rctx.path(rctx.attr.buildx)

        r = rctx.execute([buildx, "ls"])
        if not builder_name in r.stdout:
            r = rctx.execute([buildx, "create", "--name", builder_name, "--driver", "docker-container", "--use", "--bootstrap"])
            if r.return_code != 0:
                fail("Failed to create buildx driver %s: \nSTDERR:\n%s\nsSTDOUT:\n%s" % (builder_name, r.stderr, r.stdout))

    else:
        compatible_with = '["@platforms//:incompatible"]'

    rctx.file("defs.bzl", """
# Generated by configure_buildx.bzl
load("@aspect_bazel_lib//lib:run_binary.bzl", "run_binary")
load("@aspect_bazel_lib//lib:copy_file.bzl", "copy_file")

TARGET_COMPATIBLE_WITH = %s
BUILDER_NAME = "%s"

%s
%s
%s
""" % (compatible_with, builder_name, BUILDX_RULE, BUILDX_WORKAROUND, BUILDX_CONTEXT_SYNTAX_SUGAR))
    rctx.file("BUILD.bazel", '''
exports_files(["defs.bzl"])

alias(
    name = "buildx",
    actual = select({}),
    visibility = ["//visibility:public"]
)
'''.format({
        rctx.attr.buildx_platforms[platform]: str(platform)
        for platform in rctx.attr.buildx_platforms
    }))
    pass

configure_buildx = repository_rule(
    implementation = _impl_configure_buildx,
    local = True,
    attrs = {
        "buildx": attr.label(),
        "buildx_platforms": attr.label_keyed_string_dict(),
    },
)

toolchains = tag_class(attrs = {})

def _oci_extension(module_ctx):
    if len(module_ctx.modules) > 1 or len(module_ctx.modules[0].tags.toolchains) > 1:
        fail("buildx.toolchains should be called from root module only.")

    buildx_version = "0.22.0"

    buildx_platforms = {
        "linux-arm64": "6e9e455b5ec1c7ac708f2640a86c5cecce38c72e48acff6cb219dfdfa2dda781",
        "linux-amd64": "805195386fba0cea5a1487cf0d47da82a145ea0a792bd3fb477583e2dbcdcc2f",
        "darwin-arm64": "5898c338abb1f673107bc087997dc3cb63b4ea66d304ce4223472f57bd8d616e",
        "darwin-amd64": "5221ad6b8acd2283f8fbbeebc79ae4b657e83519ca1c1e4cfbb9405230b3d933",
    }

    for platform in buildx_platforms:
        http_file(
            name = "buildx_%s" % platform,
            urls = ["https://github.com/docker/buildx/releases/download/v{version}/buildx-v{version}.{platform}".format(version = buildx_version, platform = platform)],
            sha256 = buildx_platforms[platform],
            executable = True,
        )

    buildx_selects = {
        "@buildx_linux-amd64//file": "@bazel_tools//src/conditions:linux_x86_64",
        "@buildx_linux-arm64//file": "@bazel_tools//src/conditions:linux_aarch64",
        "@buildx_darwin-amd64//file": "@bazel_tools//src/conditions:darwin_x86_64",
        "@buildx_darwin-arm64//file": "@bazel_tools//src/conditions:darwin_arm64",
    }

    configure_buildx(
        name = "buildx",
        buildx = "@buildx_%s//file:downloaded" % repo_utils.platform(module_ctx).replace("_", "-"),
        buildx_platforms = buildx_selects,
    )

    root_direct_dev_deps = []

    root_direct_deps = ["buildx"]

    return module_ctx.extension_metadata(
        root_module_direct_deps = root_direct_deps,
        root_module_direct_dev_deps = root_direct_dev_deps,
    )

buildx = module_extension(
    implementation = _oci_extension,
    tag_classes = {
        "toolchains": toolchains,
    },
)
