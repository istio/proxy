"""npm packaging

Note, this is intended for sharing library code with non-Bazel consumers.

If all users of your library code use Bazel, they should just add your library
to the `deps` of one of their targets.
"""

load("@rules_nodejs//nodejs:providers.bzl", "DeclarationInfo", "JSModuleInfo", "LinkablePackageInfo", "STAMP_ATTR", "StampSettingInfo")
load("//:providers.bzl", "JSEcmaScriptModuleInfo", "JSNamedModuleInfo")

_DOC = """The pkg_npm rule creates a directory containing a publishable npm artifact.

Example:

```python
load("@build_bazel_rules_nodejs//:index.bzl", "pkg_npm")

pkg_npm(
    name = "my_package",
    srcs = ["package.json"],
    deps = [":my_typescript_lib"],
    substitutions = {"//internal/": "//"},
)
```

You can use a pair of `` comments to mark regions of files that should be elided during publishing.
For example:

```javascript
function doThing() {
    
}
```

With the Bazel stamping feature, pkg_npm will replace any placeholder version in your package with the actual version control tag.
See the [stamping documentation](https://github.com/bazelbuild/rules_nodejs/blob/master/docs/index.md#stamping)

Usage:

`pkg_npm` yields four labels. Build the package directory using the default label:

```sh
$ bazel build :my_package
Target //:my_package up-to-date:
  bazel-out/fastbuild/bin/my_package
$ ls -R bazel-out/fastbuild/bin/my_package
```

Dry-run of publishing to npm, calling `npm pack` (it builds the package first if needed):

```sh
$ bazel run :my_package.pack
INFO: Running command line: bazel-out/fastbuild/bin/my_package.pack
my-package-name-1.2.3.tgz
$ tar -tzf my-package-name-1.2.3.tgz
```

Actually publish the package with `npm publish` (also builds first):

```sh
# Check login credentials
$ bazel run @nodejs_host//:npm who
# Publishes the package
$ bazel run :my_package.publish
```

You can pass arguments to npm by escaping them from Bazel using a double-hyphen, for example:

`bazel run my_package.publish -- --tag=next`

It is also possible to use the resulting tar file file from the `.pack` as an action input via the `.tar` label.
To make use of this label, the `tgz` attribute must be set, and the generating `pkg_npm` rule must have a valid `package.json` file
as part of its sources:

```python
pkg_npm(
    name = "my_package",
    srcs = ["package.json"],
    deps = [":my_typescript_lib"],
    tgz = "my_package.tgz",
)

my_rule(
    name = "foo",
    srcs = [
        "//:my_package.tar",
    ],
)
```
"""

# Used in angular/angular /packages/bazel/src/ng_package/ng_package.bzl
PKG_NPM_ATTRS = {
    "deps": attr.label_list(
        doc = """Other targets which produce files that should be included in the package, such as `rollup_bundle`""",
        allow_files = True,
    ),
    "nested_packages": attr.label_list(
        doc = """Other pkg_npm rules whose content is copied into this package.""",
        allow_files = True,
    ),
    "package_name": attr.string(
        doc = """The package name that the linker will link this npm package as.

If package_path is set, the linker will link this package under <package_path>/node_modules/<package_name>.
If package_path is not set the this will be the root node_modules of the workspace.""",
    ),
    "package_path": attr.string(
        doc = """The package path in the workspace that the linker will link this npm package to.

If package_path is set, the linker will link this package under <package_path>/node_modules/<package_name>.
If package_path is not set the this will be the root node_modules of the workspace.""",
    ),
    "srcs": attr.label_list(
        doc = """Files inside this directory which are simply copied into the package.""",
        allow_files = True,
    ),
    "stamp": STAMP_ATTR,
    "substitutions": attr.string_dict(
        doc = """Key-value pairs which are replaced in all the files while building the package.

You can use values from the workspace status command using curly braces, for example
`{"0.0.0-PLACEHOLDER": "{STABLE_GIT_VERSION}"}`.

See the section on stamping in the [README](stamping)
""",
    ),
    "tgz": attr.string(
        doc = """If set, will create a `.tgz` file that can be used as an input to another rule, the tar will be given the name assigned to this attribute.

        NOTE: If this attribute is set, a valid `package.json` file must be included in the sources of this target
        """,
    ),
    "validate": attr.bool(
        doc = "Whether to check that the attributes match the package.json",
        default = True,
    ),
    "vendor_external": attr.string_list(
        doc = """External workspaces whose contents should be vendored into this workspace.
        Avoids `external/foo` path segments in the resulting package.""",
    ),
    "_npm_script_generator": attr.label(
        default = Label("//internal/pkg_npm:npm_script_generator"),
        cfg = "exec",
        executable = True,
    ),
    "_packager": attr.label(
        default = Label("//internal/pkg_npm:packager"),
        cfg = "exec",
        executable = True,
    ),
}

# Used in angular/angular /packages/bazel/src/ng_package/ng_package.bzl
PKG_NPM_OUTPUTS = {
    "pack_bat": "%{name}.pack.bat",
    "pack_sh": "%{name}.pack.sh",
    "publish_bat": "%{name}.publish.bat",
    "publish_sh": "%{name}.publish.sh",
}

# Takes a depset of files and returns a corresponding list of files without any files
# that aren't part of the specified package path. Also include files from external repositories
# that explicitly specified in the vendor_external list.
def _filter_out_external_files(ctx, files, package_path):
    result = []
    for file in files:
        # NB: package_path may be an empty string
        if file.short_path.startswith(package_path) and not file.short_path.startswith("../"):
            result.append(file)
        else:
            for v in ctx.attr.vendor_external:
                if file.short_path.startswith("../%s/" % v):
                    result.append(file)
    return result

# Serializes a file into a struct that matches the `BazelFileInfo` type in the
# packager implementation. Useful for transmission of such information.
def _serialize_file(file):
    return struct(path = file.path, shortPath = file.short_path)

# Serializes a list of files into a JSON string that can be passed as CLI argument
# for the packager, matching the `BazelFileInfo[]` type in the packager implementation.
def _serialize_files_for_arg(files):
    result = []
    for file in files:
        result.append(_serialize_file(file))
    return json.encode(result)

# Used in angular/angular /packages/bazel/src/ng_package/ng_package.bzl
def create_package(ctx, static_files, deps_files, nested_packages):
    """Creates an action that produces the npm package.

    It copies srcs and deps into the artifact and produces the .pack and .publish
    scripts.

    Args:
      ctx: the starlark rule context
      static_files: list of static files to be copied over into the package
      deps_files: list of files to include in the package which have been
                  specified as dependencies
      nested_packages: list of TreeArtifact outputs from other actions which are
                       to be nested inside this package

    Returns:
      The tree artifact which is the publishable directory.
    """

    stamp = ctx.attr.stamp[StampSettingInfo].value

    all_files = deps_files + static_files

    if not stamp and len(all_files) == 1 and all_files[0].is_directory and len(ctx.files.nested_packages) == 0:
        # Special case where these is a single dep that is a directory artifact and there are no
        # source files or nested_packages; in that case we assume the package is contained within
        # that single directory and there is no work to do
        package_dir = all_files[0]

        _create_npm_scripts(ctx, package_dir)

        return package_dir

    package_dir = ctx.actions.declare_directory(ctx.label.name)
    owning_package_name = ctx.label.package

    # List of dependency sources which are local to the package that defines the current
    # target. Also include files from external repositories that explicitly specified in
    # the vendor_external list. We only want to package deps files which are inside of the
    # current package unless explicitly specified.
    filtered_deps_sources = _filter_out_external_files(ctx, deps_files, owning_package_name)

    args = ctx.actions.args()
    inputs = static_files + deps_files + nested_packages

    args.use_param_file("%s", use_always = True)
    args.add(package_dir.path)
    args.add(owning_package_name)
    args.add(_serialize_files_for_arg(static_files))
    args.add(_serialize_files_for_arg(filtered_deps_sources))
    args.add(_serialize_files_for_arg(nested_packages))
    args.add(ctx.attr.substitutions)

    if stamp:
        # The version_file is an undocumented attribute of the ctx that lets us read the volatile-status.txt file
        # produced by the --workspace_status_command.
        # Similarly info_file reads the stable-status.txt file.
        # That command will be executed whenever
        # this action runs, so we get the latest version info on each execution.
        # See https://github.com/bazelbuild/bazel/issues/1054
        args.add(ctx.version_file.path)
        inputs.append(ctx.version_file)
        args.add(ctx.info_file.path)
        inputs.append(ctx.info_file)
    else:
        args.add_all(["", ""])

    args.add_joined(ctx.attr.vendor_external, join_with = ",", omit_if_empty = False)
    args.add(str(ctx.label))
    args.add(ctx.attr.validate)
    args.add(ctx.attr.package_name)

    ctx.actions.run(
        progress_message = "Assembling npm package %s" % package_dir.short_path,
        mnemonic = "AssembleNpmPackage",
        executable = ctx.executable._packager,
        inputs = inputs,
        outputs = [package_dir],
        arguments = [args],
    )

    _create_npm_scripts(ctx, package_dir)

    return package_dir

def _create_npm_scripts(ctx, package_dir):
    args = ctx.actions.args()
    toolchain = ctx.toolchains["@rules_nodejs//nodejs:toolchain_type"].nodeinfo

    args.add_all([
        package_dir.path,
        ctx.outputs.pack_sh.path,
        ctx.outputs.publish_sh.path,
        toolchain.run_npm.path,
        ctx.outputs.pack_bat.path,
        ctx.outputs.publish_bat.path,
    ])

    ctx.actions.run(
        progress_message = "Generating npm pack & publish scripts",
        mnemonic = "GenerateNpmScripts",
        executable = ctx.executable._npm_script_generator,
        inputs = [toolchain.run_npm, package_dir],
        outputs = [ctx.outputs.pack_sh, ctx.outputs.publish_sh, ctx.outputs.pack_bat, ctx.outputs.publish_bat],
        arguments = [args],
        # Must be run local (no sandbox) so that the pwd is the actual execroot
        # in the script which is used to generate the path in the pack & publish
        # scripts.
        execution_requirements = {"local": "1"},
    )

def _pkg_npm(ctx):
    deps_files_depsets = []

    for dep in ctx.attr.deps:
        # Collect whatever is in the "data"
        deps_files_depsets.append(dep.data_runfiles.files)

        # Only collect DefaultInfo files (not transitive)
        deps_files_depsets.append(dep.files)

        # All direct & transitive JavaScript-producing deps
        if JSModuleInfo in dep:
            deps_files_depsets.append(dep[JSModuleInfo].sources)

        # All direct and transitive deps that produce CommonJS modules
        if JSNamedModuleInfo in dep:
            deps_files_depsets.append(dep[JSNamedModuleInfo].sources)

        # All direct and transitive deps that produce ES6 modules
        if JSEcmaScriptModuleInfo in dep:
            deps_files_depsets.append(dep[JSEcmaScriptModuleInfo].sources)

        # Include all transitive declarations
        if DeclarationInfo in dep:
            deps_files_depsets.append(dep[DeclarationInfo].transitive_declarations)

    # Note: to_list() should be called once per rule!
    deps_files = depset(transitive = deps_files_depsets).to_list()

    package_dir = create_package(ctx, ctx.files.srcs, deps_files, ctx.files.nested_packages)

    package_dir_depset = depset([package_dir])

    result = [
        DefaultInfo(
            files = package_dir_depset,
            runfiles = ctx.runfiles([package_dir]),
        ),
    ]

    if ctx.attr.package_name:
        result.append(LinkablePackageInfo(
            package_name = ctx.attr.package_name,
            package_path = ctx.attr.package_path,
            path = package_dir.path,
            files = package_dir_depset,
        ))

    return result

pkg_npm = rule(
    implementation = _pkg_npm,
    attrs = PKG_NPM_ATTRS,
    doc = _DOC,
    outputs = PKG_NPM_OUTPUTS,
    toolchains = ["@rules_nodejs//nodejs:toolchain_type"],
)

def pkg_npm_macro(name, tgz = None, **kwargs):
    """Wrapper macro around pkg_npm

    Args:
        name: Unique name for this target
        tgz: If provided, creates a `.tar` target that can be used as an action input version of `.pack`
        **kwargs: All other args forwarded to pkg_npm
    """
    pkg_npm(
        name = name,
        **kwargs
    )

    native.alias(
        name = name + ".pack",
        actual = select({
            "@bazel_tools//src/conditions:host_windows": name + ".pack.bat",
            "//conditions:default": name + ".pack.sh",
        }),
    )

    native.alias(
        name = name + ".publish",
        actual = select({
            "@bazel_tools//src/conditions:host_windows": name + ".publish.bat",
            "//conditions:default": name + ".publish.sh",
        }),
    )

    if tgz != None:
        if not tgz.endswith(".tgz"):
            fail("tgz output for pkg_npm %s must produce a .tgz file" % name)

        native.genrule(
            name = "%s.tar" % name,
            outs = [tgz],
            # NOTE(mattem): on windows, it seems to output a bunch of other stuff on stdout when piping, so pipe to tail
            # and grab the last line
            cmd = "$(location :%s.pack) 2>/dev/null | tail -1 | xargs -I {} cp {} $@" % name,
            srcs = [
                name,
                ":%s.pack" % name,
            ],
            tags = [
                "local",
            ],
            visibility = kwargs.get("visibility"),
        )
