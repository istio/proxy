# Copyright 2022 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

"""Custom rule for creating OCPDiag Debian packages.

We build three rules: `pkg_deb_ocpdiag`, `pkg_tar_payload`, `create_version_file` in
this file. The reason of building the custom rules is because the regular build
rules, such as `pkg_tar` or `pkg_tar`, does not meet our requirements.

We want to use the custom `Make` variables to specify the version number, but there
is a pain point when we serve version variable as an argument from BUILD file.

In the regular build rules, i.e. pkg_deb, pkg_tar, they do not perform makevar
expansion of some attributes.
For example, the `package` attribute in pkg_deb can not expand the variable.
So if we build the payload package by pkg_deb, the `package` attribute will be
`<diag_name>-<version_variable_name>`, e.g. `bom-check-{DEB_VERSION}`.

To solve the above problem, we create the custom build rules for ocpdiag only
which did not serve version variable as the argument from BUILD file. Instead,
we directly get the version number from ctx.var and use it to build the debian
package, which eliminates the problem of variable expansion in the regular build rule.

See b/260288053 for more information.
"""

load("@rules_pkg//:pkg.bzl", "pkg_tar")
load(
    "@rules_pkg//:providers.bzl",
    "PackageArtifactInfo",
    "PackageFilegroupInfo",
    "PackageFilesInfo",
    "PackageSymlinkInfo",
)
load("@bazel_skylib//lib:paths.bzl", "paths")
load(
    "//ocpdiag:params_version.bzl",
    "get_change_number_command",
    "is_bazel",
)

_OVSS_PREFIX = "opt/ovss/v1/versions"
_VERSION_VAR = "DEB_VERSION"
_DEFAULT_VERSION = "1.0.0"

def _get_version_number(ctx):
    """Get version number and build metadata from environment.

    Check if there is the build metadata in the version number and split it out.
    For example, build metadata will be "502399004" if the version is
    "1.0.0+502399004", and return an empty string if version is "1.0.0".
    """
    version = ctx.var.get(_VERSION_VAR, _DEFAULT_VERSION)

    if "+" in version:
        return version.rsplit("+", 1)
    else:
        return version, ""

def _get_version_file_template_command():
    """Generates a command that creates the OCPDiag debian version file template.

    This is done by using get_change_number_command() to extract the variable
    $change_number and get the link of the corresponding change and then writing
    the version variable and the source link to a debian version file template.

    Returns:
        A bash script that create the version file template as a string to be included
        in a genrule.
    """
    if is_bazel():
        #
        url = "https://ocpdiag-review.git.corp.google.com/c/ocpdiag/+/$${change_number}"
    else:
        url = "https://critique.corp.google.com/cl/$${change_number}"

    change_number_cmd = get_change_number_command()
    build_version_file_cmd = """
    echo "Version: {version_var}\nSource: {url}" >> $@
    """.format(
        version_var = _VERSION_VAR,
        url = url,
    )
    return change_number_cmd + build_version_file_cmd

def _get_description_file_template_command(description):
    """Generates a command that creates the OCPDiag debian description file template.

    This is done by using get_change_number_command() to extract the variable
    $change_number and get the link of the corresponding change and then writing
    the description and the source link to a debian description file template.

    Args:
      description: The description for the debian package.

    Returns:
        A bash script that create the description file template as a string to be included
        in a genrule.
    """
    if is_bazel():
        #
        url = "https://ocpdiag-review.git.corp.google.com/c/ocpdiag/+/$${change_number}"
    else:
        url = "https://critique.corp.google.com/cl/$${change_number}"

    change_number_cmd = get_change_number_command()
    build_version_file_cmd = """
    echo "{description} Source: {url}" >> $@
    """.format(
        description = description,
        url = url,
    )
    return change_number_cmd + build_version_file_cmd

def _expand_template_impl(ctx):
    """Replace the version variable in template file with the version number.

    Args:
      ctx: Execution context for the BUILD rule.

    Returns:
      A version file which contains version number and the link to the corresponding
      change.
    """
    in_file = ctx.file.input
    out_file = ctx.actions.declare_file(ctx.attr.name)

    deb_version, _ = _get_version_number(ctx)
    ctx.actions.expand_template(
        template = in_file,
        output = out_file,
        substitutions = {
            _VERSION_VAR: deb_version,
        },
    )

    return [DefaultInfo(
        files = depset([out_file]),
        runfiles = ctx.runfiles(files = [out_file]),
    )]

_expand_template = rule(
    implementation = _expand_template_impl,
    attrs = {
        "input": attr.label(
            mandatory = True,
            allow_single_file = True,
            doc = "The template file to expand.",
        ),
    },
)

def create_version_file(name):
    """Create the version file which contains the version number and the link.

    Args:
      name: Name of the version file build rule.
    """
    template = "_%s_template" % name
    native.genrule(
        name = template,
        outs = ["_%s_template.tpl" % name],
        cmd = _get_version_file_template_command(),
        stamp = 1,
        visibility = ["//visibility:private"],
    )
    _expand_template(
        name = name,
        input = ":" + template,
    )

def create_description_file(name, description):
    """Create the description file which contains the description and the link.

    Args:
      name: Name of the version file build rule.
      description: The description for the debian package.
    """
    template = "_%s_template" % name
    native.genrule(
        name = template,
        outs = ["_%s_template.tpl" % name],
        cmd = _get_description_file_template_command(description),
        stamp = 1,
        visibility = ["//visibility:private"],
    )
    _expand_template(
        name = name,
        input = ":" + template,
    )

def join_paths(path, *others):
    return paths.normalize(paths.join(path, *others))

#
def _pkg_tar_runfiles_for_deb_impl(ctx):
    """The implementation for the _pkg_tar_runfiles_for_deb rule."""
    deb_version, _ = _get_version_number(ctx)
    package_dir = join_paths(_OVSS_PREFIX, ctx.attr.test_name, deb_version)

    runfiles = ctx.runfiles()
    files = []
    for src in [s[DefaultInfo] for s in ctx.attr.srcs]:
        runfiles = runfiles.merge(src.default_runfiles)
        files.append(src.files)

    runfiles_files = runfiles.files.to_list()
    workspace_prefix = join_paths(ctx.attr.runfiles_prefix, ctx.workspace_name)
    manifest = PackageFilegroupInfo(
        pkg_files = [(files_info, ctx.label) for files_info in [
            PackageFilesInfo(dest_src_map = {
                join_paths(
                    package_dir,
                    workspace_prefix,
                    file.owner.workspace_root,
                    file.short_path,
                ): file
                for file in runfiles_files
            }),
        ]],
        # Declare runfiles symlinks
        pkg_symlinks = [(sym_info, ctx.label) for sym_info in [
            PackageSymlinkInfo(
                destination = join_paths(package_dir, sym),
                target = join_paths(workspace_prefix, ctx.attr.symlinks[sym]),
            )
            for sym in ctx.attr.symlinks
        ] + [
            # Add top-level symlinks to external repos' runfiles.
            PackageSymlinkInfo(
                destination = join_paths(package_dir, ctx.attr.runfiles_prefix, name),
                target = join_paths(ctx.workspace_name, path),
            )
            for name, path in {
                file.owner.workspace_name: file.owner.workspace_root
                for file in runfiles_files
                if file.owner.workspace_root
            }.items()
        ]],
        pkg_dirs = [],  # pkg_* rules do not gracefully handle empty fields.
    )

    return [
        manifest,
        DefaultInfo(files = depset(transitive = files + [runfiles.files])),
    ]

_pkg_tar_runfiles_for_deb = rule(
    implementation = _pkg_tar_runfiles_for_deb_impl,
    provides = [PackageFilegroupInfo],
    attrs = {
        "srcs": attr.label_list(
            mandatory = True,
            doc = "Target to include in the tarball.",
        ),
        "test_name": attr.string(
            mandatory = True,
            doc = "The name of the OCPDiag test.",
        ),
        "runfiles_prefix": attr.string(
            doc = "Path prefix for the runfiles directory.",
            default = ".",
        ),
        "symlinks": attr.string_dict(
            doc = "Symlinks to include in the debian package.",
            default = {},
        ),
    },
)

# The regular pkg_tar rule does not perform makevar expansion of the path in symlink.
# Creates pkg_tar_payload for building ocpdiag payload tar archive only.
def pkg_tar_payload(name, test_name, srcs, symlinks, **kwargs):
    """Packages a ocpdiag launcher and version file into payload tar archive.

    Args:
      name: Name of debian package build rule.
      test_name: The name of the OCPDiag test.
      srcs: Target to include in the tarball.
      symlinks: Symlinks to include in the tarball.
      **kwargs:  Any arguments to be forwarded to pkg_tar.
    """
    runfiles = "_%s_runfiles" % name
    _pkg_tar_runfiles_for_deb(
        name = runfiles,
        test_name = test_name,
        srcs = srcs,
        runfiles_prefix = "%s.runfiles" % test_name,
        symlinks = symlinks,
    )

    pkg_tar(
        name = name,
        srcs = [":" + runfiles],
        extension = "tar.gz",
        mode = "0755",
        package_dir = ".",
        **kwargs
    )

def _gen_deb_impl(ctx, name, data, package, version, depends = None):
    output_file = ctx.actions.declare_file("%s.deb" % name)
    changes_file = ctx.actions.declare_file("%s.changes" % name)

    args = ctx.actions.args()
    args.add("--output", output_file)
    args.add("--changes", changes_file)
    args.add("--data", data)
    args.add("--package", package)
    args.add("--version", version)
    args.add("--architecture", ctx.attr.architecture)
    args.add("--maintainer", ctx.attr.maintainer)
    args.add("--distribution", ctx.attr.distribution)
    args.add("--urgency", ctx.attr.urgency)
    args.add("--description=@" + ctx.file.description_file.path)

    if depends:
        args.add("--depends", depends)

    ctx.actions.run(
        mnemonic = "MakeDeb",
        executable = ctx.executable._make_deb,
        arguments = [args],
        inputs = [data, ctx.file.description_file],
        outputs = [output_file, changes_file],
        env = {
            "LANG": "en_US.UTF-8",
            "LC_CTYPE": "UTF-8",
            "PYTHONIOENCODING": "UTF-8",
            "PYTHONUTF8": "1",
        },
    )
    return output_file, changes_file

def _pkg_deb_ocpdiag_impl(ctx):
    """The implementation for the pkg_deb_ocpdiag rule."""
    deb_version, build_metadata = _get_version_number(ctx)

    # Build pointer package
    if ctx.attr.pointer_name:
        pointer_name = ctx.attr.pointer_name
    else:
        pointer_name = "%s_%s_%s" % (ctx.attr.package, deb_version, ctx.attr.architecture)
    pointer_output_file, pointer_changes_file = _gen_deb_impl(
        ctx = ctx,
        name = pointer_name,
        data = ctx.file.pointer_data,
        package = ctx.attr.package,
        version = "%s+%s" % (deb_version, build_metadata) if build_metadata else deb_version,
        depends = "%s-%s" % (ctx.attr.package, deb_version),
    )

    # Build payload package
    if ctx.attr.payload_name:
        payload_name = ctx.attr.payload_name
    else:
        payload_name = "%s-%s_0.0_%s" % (ctx.attr.package, deb_version, ctx.attr.architecture)
    payload_output_file, payload_changes_file = _gen_deb_impl(
        ctx = ctx,
        name = payload_name,
        data = ctx.file.payload_data,
        package = "%s-%s" % (ctx.attr.package, deb_version),
        version = "0.0",
    )

    outputs = [pointer_output_file, payload_output_file]
    changes = [pointer_changes_file, payload_changes_file]
    return [
        OutputGroupInfo(
            deb = outputs,
            changes = changes,
        ),
        DefaultInfo(
            files = depset(outputs),
            runfiles = ctx.runfiles(files = outputs),
        ),
        PackageArtifactInfo(
            label = ctx.label.name,
            file = outputs,
        ),
    ]

# The regular pkg_deb rule does not perform makevar expansion of the attribute
# "package". Creates pkg_deb_ocpdiag for building ocpdiag payload tar archive only.
pkg_deb_ocpdiag = rule(
    implementation = _pkg_deb_ocpdiag_impl,
    attrs = {
        "pointer_data": attr.label(
            doc = "A tar file that contains the pointer data.",
            mandatory = True,
            allow_single_file = [".tar", ".tar.gz", ".tgz", ".tar.bz2", "tar.xz"],
        ),
        "payload_data": attr.label(
            doc = "A tar file that contains the payload data.",
            mandatory = True,
            allow_single_file = [".tar", ".tar.gz", ".tgz", ".tar.bz2", "tar.xz"],
        ),
        "package": attr.string(
            doc = "The name of the package.",
            mandatory = True,
        ),
        "architecture": attr.string(
            doc = "Package architecture.",
            mandatory = True,
        ),
        "maintainer": attr.string(
            doc = "The maintainer of the package.",
            mandatory = True,
        ),
        "description_file": attr.label(
            doc = "The package description file.",
            allow_single_file = True,
            mandatory = True,
        ),
        "distribution": attr.string(
            doc = "distribution: See http://www.debian.org/doc/debian-policy.",
            default = "unstable",
        ),
        "urgency": attr.string(
            doc = "urgency: See http://www.debian.org/doc/debian-policy.",
            default = "medium",
        ),
        "pointer_name": attr.string(
            doc = """The name of the pointer package.
          Default: "{package}_{version}_{architecture}.deb""",
        ),
        "payload_name": attr.string(
            doc = """The name of the payload package.
          Default: "{package}-{version}_0.0_{architecture}.deb""",
        ),

        # Implicit dependencies.
        "_make_deb": attr.label(
            default = Label("@rules_pkg//pkg/private/deb:make_deb"),
            cfg = "exec",
            executable = True,
            allow_files = True,
        ),
    },
    provides = [PackageArtifactInfo],
)
