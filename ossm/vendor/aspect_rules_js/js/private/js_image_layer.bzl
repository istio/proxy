"""Rules for creating container image layers from js_binary targets

For example, this js_image_layer target outputs `node_modules.tar` and `app.tar` with `/app` prefix.

```starlark
load("@aspect_rules_js//js:defs.bzl", "js_image_layer")

js_image_layer(
    name = "layers",
    binary = "//label/to:js_binary",
    root = "/app",
)
```
"""

load("@aspect_bazel_lib//lib:paths.bzl", "to_rlocation_path")
load("@aspect_bazel_lib//lib:utils.bzl", "is_bazel_6_or_greater")
load("@bazel_skylib//lib:paths.bzl", "paths")

_DOC = """Create container image layers from js_binary targets.

By design, js_image_layer doesn't have any preference over which rule assembles the container image. 
This means the downstream rule (`oci_image`, or `container_image` in this case) must set a proper `workdir` and `cmd` to for the container work.
A proper `cmd` usually looks like /`[ root of js_image_layer ]`/`[ relative path to BUILD file from WORKSPACE or package_name() ]/[ name of js_binary ]`, 
unless you have a launcher script that invokes the entry_point of the `js_binary` in a different path.
On the other hand, `workdir` has to be set to `runfiles tree root` which would be exactly `cmd` **but with `.runfiles/[ name of the workspace or __main__ if empty ]` suffix**. If `workdir` is not set correctly, some
attributes such as `chdir` might not work properly.

js_image_layer supports transitioning to specific `platform` to allow building multi-platform container images.

> WARNING: Structure of the resulting layers are not subject to semver guarantees and may change without a notice. However, it is guaranteed to work when provided together in the `app` and `node_modules` order

**A partial example using rules_oci with transition to linux/amd64 platform.**

```starlark
load("@aspect_rules_js//js:defs.bzl", "js_binary", "js_image_layer")
load("@rules_oci//oci:defs.bzl", "oci_image")

js_binary(
    name = "binary",
    entry_point = "main.js",
)

platform(
    name = "amd64_linux",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
)

js_image_layer(
    name = "layers",
    binary = ":binary",
    platform = ":amd64_linux",
    root = "/app"
)

oci_image(
    name = "image",
    cmd = ["/app/main"],
    entrypoint = ["bash"],
    tars = [
        ":layers"
    ]
)
```

**A partial example using rules_oci to create multi-platform images.**


```starlark
load("@aspect_rules_js//js:defs.bzl", "js_binary", "js_image_layer")
load("@rules_oci//oci:defs.bzl", "oci_image", "oci_image_index")

js_binary(
    name = "binary",
    entry_point = "main.js",
)

[
    platform(
        name = "linux_{}".format(arch),
        constraint_values = [
            "@platforms//os:linux",
            "@platforms//cpu:{}".format(arch if arch != "amd64" else "x86_64"),
        ],
    )
    js_image_layer(
        name = "{}_layers".format(arch),
        binary = ":binary",
        platform = ":linux_{arch}",
        root = "/app"
    )
    oci_image(
        name = "{}_image".format(arch),
        cmd = ["/app/main"],
        entrypoint = ["bash"],
        tars = [
            ":{}_layers".format(arch)
        ]
    )
    for arch in ["amd64", "arm64"]
]

oci_image_index(
    name = "image",
    images = [
        ":arm64_image",
        ":amd64_image"
    ]
)

```

**An example using legacy rules_docker**

See `e2e/js_image_rules_docker` for full example.

```starlark
load("@aspect_rules_js//js:defs.bzl", "js_binary", "js_image_layer")
load("@io_bazel_rules_docker//container:container.bzl", "container_image")

js_binary(
    name = "main",
    data = [
        "//:node_modules/args-parser",
    ],
    entry_point = "main.js",
)


js_image_layer(
    name = "layers",
    binary = ":main",
    root = "/app",
    visibility = ["//visibility:__pkg__"],
)

filegroup(
    name = "app_tar", 
    srcs = [":layers"], 
    output_group = "app"
)
container_layer(
    name = "app_layer",
    tars = [":app_tar"],
)

filegroup(
    name = "node_modules_tar", 
    srcs = [":layers"], 
    output_group = "node_modules"
)
container_layer(
    name = "node_modules_layer",
    tars = [":node_modules_tar"],
)

container_image(
    name = "image",
    cmd = ["/app/main"],
    entrypoint = ["bash"],
    layers = [
        ":app_layer",
        ":node_modules_layer",
    ],
)
```
"""

# BAZEL_BINDIR has to be set to '.' so that js_binary preserves the PWD when running inside container.
# See https://github.com/aspect-build/rules_js/tree/dbb5af0d2a9a2bb50e4cf4a96dbc582b27567155#running-nodejs-programs
# for why this is needed.
_LAUNCHER_TMPL = """\
#!/usr/bin/env bash
export BAZEL_BINDIR=.
source {executable_path}
"""

def _write_laucher(ctx, executable_path):
    "Creates a call-through shell entrypoint which sets BAZEL_BINDIR to '.' then immediately invokes the original entrypoint."
    launcher = ctx.actions.declare_file("%s_launcher.sh" % ctx.label.name)
    ctx.actions.write(
        output = launcher,
        content = _LAUNCHER_TMPL.format(executable_path = executable_path),
        is_executable = True,
    )
    return launcher

def _runfile_path(ctx, file, runfiles_dir):
    return paths.join(runfiles_dir, to_rlocation_path(ctx, file))

def _runfiles_dir(root, default_info):
    manifest = default_info.files_to_run.runfiles_manifest

    nobuild_runfile_links_is_set = manifest.short_path.endswith("_manifest")

    if nobuild_runfile_links_is_set:
        # When `--nobuild_runfile_links` is set, runfiles_manifest points to the manifest
        # file sitting adjacent to the runfiles tree rather than within it.
        runfiles = default_info.files_to_run.runfiles_manifest.short_path.replace("_manifest", "")
    else:
        runfiles = manifest.short_path.replace(manifest.basename, "")[:-1]

    return paths.join(root, runfiles.replace(".sh", ""))

def _build_layer(ctx, type, entries, inputs):
    entries_output = ctx.actions.declare_file("{}_{}_entries.json".format(ctx.label.name, type))
    ctx.actions.write(entries_output, content = json.encode(entries))

    extension = "tar.gz" if ctx.attr.compression == "gzip" else "tar"
    output = ctx.actions.declare_file("{name}_{type}.{extension}".format(name = ctx.label.name, type = type, extension = extension))

    args = ctx.actions.args()
    args.add(entries_output)
    args.add(output)
    args.add(ctx.attr.compression)
    args.add(ctx.attr.owner)
    if not is_bazel_6_or_greater():
        args.add("true")

    ctx.actions.run(
        inputs = inputs + [entries_output],
        outputs = [output],
        arguments = [args],
        executable = ctx.executable._builder,
        progress_message = "JsImageLayer %{label}",
        mnemonic = "JsImageLayer",
        env = {
            "BAZEL_BINDIR": ".",
        },
    )

    return output

def _should_be_in_node_modules_layer(destination, file):
    is_node = file.owner.workspace_name != "" and "/bin/nodejs/" in destination
    is_node_modules = "/node_modules/" in destination
    is_js_patches = "/js/private/node-patches" in destination
    return is_node or is_node_modules or is_js_patches

def _js_image_layer_impl(ctx):
    if len(ctx.attr.binary) != 1:
        fail("binary attribute has more than one transition")

    ownersplit = ctx.attr.owner.split(":")
    if len(ownersplit) != 2 or not ownersplit[0].isdigit() or not ownersplit[1].isdigit():
        fail("owner attribute should be in `0:0` `int:int` format.")

    default_info = ctx.attr.binary[0][DefaultInfo]
    runfiles_dir = _runfiles_dir(ctx.attr.root, default_info)

    executable = default_info.files_to_run.executable
    executable_path = paths.replace_extension(paths.join(ctx.attr.root, executable.short_path), "")
    real_executable_path = _runfile_path(ctx, executable, runfiles_dir)
    launcher = _write_laucher(ctx, real_executable_path)

    all_files = depset(transitive = [default_info.files, default_info.default_runfiles.files])

    app_entries = {executable_path: {"dest": launcher.path, "root": launcher.root.path}}
    app_inputs = [launcher]

    node_modules_entries = {}
    node_modules_inputs = []

    for file in all_files.to_list():
        destination = _runfile_path(ctx, file, runfiles_dir)
        entry = {
            "dest": file.path,
            "root": file.root.path,
            "is_external": file.owner.workspace_name != "",
            "is_source": file.is_source,
            "is_directory": file.is_directory,
        }
        if destination == real_executable_path:
            entry["remove_non_hermetic_lines"] = True

        if _should_be_in_node_modules_layer(destination, file):
            node_modules_entries[destination] = entry
            node_modules_inputs.append(file)
        else:
            app_entries[destination] = entry
            app_inputs.append(file)

    app = _build_layer(ctx, type = "app", entries = app_entries, inputs = app_inputs)
    node_modules = _build_layer(ctx, type = "node_modules", entries = node_modules_entries, inputs = node_modules_inputs)

    return [
        DefaultInfo(files = depset([app, node_modules])),
        OutputGroupInfo(app = depset([app]), node_modules = depset([node_modules])),
    ]

def _js_image_layer_transition_impl(settings, attr):
    # buildifier: disable=unused-variable
    _ignore = (settings)
    if not attr.platform:
        return {}
    return {
        "//command_line_option:platforms": str(attr.platform),
    }

_js_image_layer_transition = transition(
    implementation = _js_image_layer_transition_impl,
    inputs = [],
    outputs = ["//command_line_option:platforms"],
)

js_image_layer_lib = struct(
    implementation = _js_image_layer_impl,
    attrs = {
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
        "_builder": attr.label(
            default = "//js/private:js_image_layer_builder",
            executable = True,
            cfg = "exec",
        ),
        "binary": attr.label(
            mandatory = True,
            cfg = _js_image_layer_transition,
            doc = "Label to an js_binary target",
        ),
        "root": attr.string(
            doc = "Path where the files from js_binary will reside in. eg: /apps/app1 or /app",
        ),
        "owner": attr.string(
            doc = "Owner of the entries, in `GID:UID` format. By default `0:0` (root, root) is used.",
            default = "0:0",
        ),
        "compression": attr.string(
            doc = "Compression algorithm. Can be one of `gzip`, `none`.",
            values = ["gzip", "none"],
            default = "gzip",
        ),
        "platform": attr.label(
            doc = "Platform to transition.",
        ),
    },
)

js_image_layer = rule(
    implementation = js_image_layer_lib.implementation,
    attrs = js_image_layer_lib.attrs,
    doc = _DOC,
)
