"""Load an oci_image into runtimes such as podman and docker.
Intended for use with `bazel run`.

For example, given an `:image` target, you could write

```
oci_load(
    name = "load",
    image = ":image",
    repo_tags = ["my-repository:latest"],
)
```

and then run it in a container like so:

```
bazel run :load
docker run --rm my-repository:latest
```
"""

load("@aspect_bazel_lib//lib:paths.bzl", "BASH_RLOCATION_FUNCTION", "to_rlocation_path")
load("//oci/private:util.bzl", "util")

doc = """Loads an OCI layout into a container daemon without needing to publish the image first.

Passing anything other than oci_image to the image attribute will lead to build time errors.

### Build Outputs

The default output is an mtree specification file.
This is because producing the tarball in `bazel build` is expensive, and should typically not be an input to any other build actions,
so producing it only creates unnecessary load on the action cache.

If needed, the `tarball` output group allows you to depend on the tar output from another rule.

On the command line, `bazel build //path/to:my_tarball --output_groups=+tarball`

or in a BUILD file:

```starlark
oci_load(
    name = "my_tarball",
    ...
)
filegroup(
    name = "my_tarball.tar",
    srcs = [":my_tarball"],
    output_group = "tarball",
)
```

### When using `format = "oci"`

When using format = oci, containerd image store needs to be enabled in order for the oci style tarballs to work. 

On docker desktop this can be enabled by visiting `Settings (cog icon) -> Features in development -> Use containerd for pulling and storing images`

For more information, see https://docs.docker.com/desktop/containerd/

### Multiple images

To load more than one image into the daemon,
use [rules_multirun] to group multiple oci_load targets into one executable target.

This might be useful with a docker-compose workflow, for example.

```starlark
load("@rules_multirun//:defs.bzl", "command", "multirun")

IMAGES = {
    "webservice": "//path/to/web-service:image.load",
    "backend": "//path/to/backend-service:image.load",
}

[
    command(
        name = k,
        command = v,
    )
    for (k, v) in IMAGES.items()
]

multirun(
    name = "load_all",
    commands = IMAGES.keys(),
)
```

[rules_multirun]: https://github.com/keith/rules_multirun
"""

attrs = {
    "format": attr.string(
        default = "docker",
        doc = "Format of image to generate. Options are: docker, oci. Currently, when the input image is an image_index, only oci is supported, and when the input image is an image, only docker is supported. Conversions between formats may be supported in the future.",
        values = ["docker", "oci"],
    ),
    "image": attr.label(mandatory = True, allow_single_file = True, doc = "Label of a directory containing an OCI layout, typically `oci_image`"),
    "repo_tags": attr.label(
        doc = """\
            a file containing repo_tags, one per line.
            """,
        allow_single_file = [".txt"],
        mandatory = True,
    ),
    "loader": attr.label(
        doc = """\
            Alternative target for a container cli tool that will be
            used to load the image into the local engine when using `bazel run` on this target.

            By default, we look for `docker` or `podman` or `nerdctl` on the PATH, and run the `load` command.

            See the _run_template attribute for the script that calls this loader tool.
            """,
        allow_files = True,
        mandatory = False,
        executable = True,
        cfg = "target",
    ),
    "_run_template": attr.label(
        default = Label("//oci/private:load.sh.tpl"),
        doc = """ \
              The template used to load the container when using `bazel run` on this target.

              See the `loader` attribute to replace the tool which is called.
              Please reference the default template to see available substitutions.
        """,
        allow_single_file = True,
    ),
    "_tarball_sh": attr.label(allow_single_file = True, default = "//oci/private:tarball.sh.tpl"),
    "_runfiles": attr.label(default = "@bazel_tools//tools/bash/runfiles"),
} | util.IS_EXEC_PLATFORM_WINDOWS_ATTRS

def _get_workspace_name(ctx, file):
    label = getattr(file, "owner", None)
    if label and getattr(label, "workspace_name", None):
        return label.workspace_name
    return ctx.workspace_name

def _get_runfiles_prefix(ctx, file):
    return _get_workspace_name(ctx, file) + "/"

def _get_workspace_root_path(ctx, file):
    ws = _get_workspace_name(ctx, file)
    if ws and ws != ctx.workspace_name:
        return file.root.path + "/external/" + ws
    return file.root.path

def _load_impl(ctx):
    jq = ctx.toolchains["@aspect_bazel_lib//lib:jq_toolchain_type"]
    coreutils = ctx.toolchains["@aspect_bazel_lib//lib:coreutils_toolchain_type"]
    bsdtar = ctx.toolchains["@tar.bzl//tar/toolchain:type"]
    bsdtar_target = ctx.toolchains["@tar.bzl//tar/toolchain:target_type"]

    image = ctx.file.image
    repo_tags = ctx.file.repo_tags

    mtree_spec = ctx.actions.declare_file("{}/tarball.spec".format(ctx.label.name))
    executable = ctx.actions.declare_file("{}/load.sh".format(ctx.label.name))
    manifest_json = ctx.actions.declare_file("{}/manifest.json".format(ctx.label.name))

    # Represents either manifest.json or index.json depending on the image format
    substitutions = {
        "{{format}}": ctx.attr.format,
        "{{jq_path}}": jq.jqinfo.bin.path,
        "{{coreutils_path}}": coreutils.coreutils_info.bin.path,
        "{{tar}}": bsdtar.tarinfo.binary.path,
        "{{image_dir}}": image.path,
        "{{output}}": mtree_spec.path,
        "{{json_out}}": manifest_json.path,
    }

    if ctx.attr.repo_tags:
        substitutions["{{tags}}"] = repo_tags.path

    ctx.actions.expand_template(
        template = ctx.file._tarball_sh,
        output = executable,
        is_executable = True,
        substitutions = substitutions,
    )

    mtree_inputs = depset(
        direct = [image, repo_tags, executable],
        transitive = [bsdtar.default.files],
    )
    mtree_outputs = [mtree_spec, manifest_json]
    ctx.actions.run(
        executable = util.maybe_wrap_launcher_for_windows(ctx, executable),
        inputs = mtree_inputs,
        outputs = mtree_outputs,
        tools = [
            jq.jqinfo.bin,
            coreutils.coreutils_info.bin,
        ],
        mnemonic = "OCITarballManifest",
    )

    # This action produces a large output and should rarely be used as it puts load on the cache.
    # It will only run if the "tarball" output_group is explicitly requested
    tarball = ctx.actions.declare_file("{}/tarball.tar".format(ctx.label.name))
    tar_inputs = depset(direct = mtree_outputs, transitive = [mtree_inputs, bsdtar_target.default.files])
    tar_args = ctx.actions.args()
    tar_args.add_all(["--create", "--no-xattr", "--no-mac-metadata"])
    tar_args.add("--file", tarball)
    tar_args.add(mtree_spec, format = "@%s")
    ctx.actions.run(
        executable = bsdtar.tarinfo.binary,
        inputs = tar_inputs,
        outputs = [tarball],
        arguments = [tar_args],
        mnemonic = "OCITarball",
    )

    # Create an executable runner script that will create the tarball at runtime,
    # as opposed to at build to avoid uploading large artifacts to remote cache.
    runnable_loader = ctx.actions.declare_file(ctx.label.name + ".sh")

    runtime_deps = []
    if ctx.executable.loader:
        runtime_deps.append(ctx.executable.loader)
    runfiles = ctx.runfiles(runtime_deps, transitive_files = tar_inputs)
    runfiles = runfiles.merge(ctx.attr.image[DefaultInfo].default_runfiles)
    runfiles = runfiles.merge(ctx.attr._runfiles.default_runfiles)
    if ctx.executable.loader:
        runfiles = runfiles.merge(ctx.attr.loader.default_runfiles)

    ctx.actions.expand_template(
        template = ctx.file._run_template,
        output = runnable_loader,
        substitutions = {
            "{{BASH_RLOCATION_FUNCTION}}": BASH_RLOCATION_FUNCTION,
            "{{tar}}": to_rlocation_path(ctx, bsdtar_target.tarinfo.binary),
            "{{mtree_path}}": to_rlocation_path(ctx, mtree_spec),
            "{{loader}}": to_rlocation_path(ctx, ctx.executable.loader) if ctx.executable.loader else "",
            # This rule could be declared in external workspace than current execution context(e.g. main_wksp -> external_wksp -> rules_oci).
            # In such cases we need to handle manifest_json and image in a workspace-aware way(when --nolegacy_external_runfiles flag is set).
            "{{manifest_root}}": _get_workspace_root_path(ctx, manifest_json),
            "{{image_root}}": _get_workspace_root_path(ctx, image),
            "{{image_runfiles_prefix}}": _get_runfiles_prefix(ctx, image),
            "{{manifest_runfiles_prefix}}": _get_runfiles_prefix(ctx, manifest_json),
        },
        is_executable = True,
    )

    return [
        DefaultInfo(
            runfiles = runfiles,
            executable = runnable_loader,
        ),
        OutputGroupInfo(tarball = depset([tarball])),
    ]

oci_load = rule(
    implementation = _load_impl,
    attrs = attrs,
    doc = doc,
    toolchains = [
        "@bazel_tools//tools/sh:toolchain_type",
        "@aspect_bazel_lib//lib:coreutils_toolchain_type",
        "@aspect_bazel_lib//lib:jq_toolchain_type",
        "@tar.bzl//tar/toolchain:type",
        "@tar.bzl//tar/toolchain:target_type",
    ],
    executable = True,
)
