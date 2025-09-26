# Copyright 2022 Google LLC
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

"""Bazel rules for building and packaging OCPDiag tests"""

load("@rules_pkg//:pkg.bzl", "pkg_tar")
load("@rules_pkg//:providers.bzl", "PackageFilegroupInfo", "PackageFilesInfo", "PackageSymlinkInfo")
load("@bazel_skylib//lib:paths.bzl", "paths")
load(
    "//ocpdiag:debian.bzl",
    "create_description_file",
    "create_version_file",
    "pkg_deb_ocpdiag",
    "pkg_tar_payload",
)
load("@rules_proto//proto:defs.bzl", "ProtoInfo")

_OCPDIAG_WORKSPACE = "ocpdiag"

def _parse_label(label):
    """Parse a label into (package, name).

    Args:
      label: string in relative or absolute form.

    Returns:
      Pair of strings: package, relative_name

    Raises:
      ValueError for malformed label (does not do an exhaustive validation)
    """
    if label.startswith("//"):
        label = label[2:]  # drop the leading //
        colon_split = label.split(":")
        if len(colon_split) == 1:  # no ":" in label
            pkg = label
            _, _, target = label.rpartition("/")
        else:
            pkg, target = colon_split  # fails if len(colon_split) != 2
    else:
        colon_split = label.split(":")
        if len(colon_split) == 1:  # no ":" in label
            pkg, target = native.package_name(), label
        else:
            pkg2, target = colon_split  # fails if len(colon_split) != 2
            pkg = native.package_name() + ("/" + pkg2 if pkg2 else "")
    return pkg, target

def _descriptor_set_impl(ctx):
    """Create a FileDescriptorSet with the transitive inclusions of the passed protos.

    Emulates the output of `protoc --include_imports --descriptor_set_out=$@`
    In Bazel that would require recompiling protoc, so use the existing ProtoInfo.
    """
    output = ctx.actions.declare_file(ctx.attr.name + "-transitive-descriptor-set.proto.bin")
    descriptors = depset(transitive = [
        dep[ProtoInfo].transitive_descriptor_sets
        for dep in ctx.attr.deps
    ])
    ctx.actions.run_shell(
        outputs = [output],
        inputs = descriptors,
        arguments = [file.path for file in descriptors.to_list()],
        command = "cat $@ >%s" % output.path,
    )
    return DefaultInfo(
        files = depset([output]),
        runfiles = ctx.runfiles(files = [output]),
    )

descriptor_set = rule(
    attrs = {"deps": attr.label_list(providers = [ProtoInfo])},
    implementation = _descriptor_set_impl,
)

def _master_rule_impl(ctx):
    """Combines two rules into a single target.

    Used to instantiate gendeb and export_dependencies rule with one target.

    Args:
      ctx: Rule's context

    Returns:
      Union of the output files of both rules
    """
    return DefaultInfo(files = depset(transitive = [ctx.attr.dep_rule_1.files, ctx.attr.dep_rule_2.files]))

master_rule = rule(
    implementation = _master_rule_impl,
    attrs = {
        "dep_rule_1": attr.label(),
        "dep_rule_2": attr.label(),
    },
)

_DEFAULT_PROTO = "@com_google_protobuf//:empty_proto"

def ocpdiag_test_pkg(
        name,
        binary,
        params_proto = _DEFAULT_PROTO,
        json_defaults = None,
        tags = [],
        **kwargs):
    """Packages a ocpdiag binary into a self-extracting launcher.

    Args:
      name: Name of the launcher.
      binary: Executable program for the ocpdiag test; must include all data deps.
      params_proto: proto_library rule for the input parameters.
      json_defaults: Optional JSON file with default values for the params.
      tags: Tags that will be applied to generated build targets.
      **kwargs: Additional args to pass to the shell binary rule.
    """
    descriptors = "_ocpdiag_test_pkg_%s_descriptors" % name
    descriptor_set(
        name = descriptors,
        deps = [params_proto],
        visibility = ["//visibility:private"],
    )

    launcher = "_ocpdiag_test_pkg_%s_launcher" % name
    ocpdiag_launcher(
        name = launcher,
        binary = binary,
        descriptors = ":%s" % descriptors,
        defaults = json_defaults,
        visibility = ["//visibility:private"],
        tags = tags,
    )

    ocpdiag_sar_name = name

    create_ocpdiag_sar(
        name = ocpdiag_sar_name,
        launcher = ":" + launcher,
        tags = tags,
    )

def ocpdiag_ovss_tar(name, ocpdiag_test, **kwargs):
    """Packages a OCPDiag tarball from a OCPDiag package.

    Args:
      name: Name of the tarball build rule.
      ocpdiag_test: OCPDiag package.
      **kwargs: Additional args to pass to the pkg_tar rule.
    """
    _, test_name = _parse_label(ocpdiag_test)
    ovss_prefix = "export/hda3/ocpdiag/%s" % test_name
    pkg_tar(
        name = name,
        srcs = [ocpdiag_test],
        mode = "0755",
        package_dir = ovss_prefix,
        out = "%s.tar" % test_name,
        **kwargs
    )

def ocpdiag_ovss_debian(
        name,
        description,
        ocpdiag_test,
        is_ocpdiag = False,
        json_defaults = None):
    """Packages a ocpdiag binary into debian packages for release in OVSS.

    This creates the payload package containing the binary and runfiles themselves and
    the pointer package, which contains a dependency on the payload package and exists
    to be included in OVSS release bundles.

    To specify the version, add the flag `--define DEB_VERSION=<version>`.
    (e.g., `--define DEB_VERSION=1.2.3`)

    Args:
      name: Name of debian package build rule.
      description: The description for the debian package.
      ocpdiag_test: OCPDiag package.
      json_defaults: Optional JSON file with default values for the params.
      is_ocpdiag: Whether the diagnostic is a ocpdiag.
    """

    # arch_constraints contains the mapping of acceptable architecture and its cpu constraints.
    arch_constraints = {
        "amd64": ["@platforms//cpu:x86_64"],
        "arm64": ["@platforms//cpu:aarch64"],
    }

    _, ocpdiag_test_name = _parse_label(ocpdiag_test)
    debian_test_name = ocpdiag_test_name.replace("_", "-")

    # Select ocpdiag binary
    binary = ocpdiag_test_name if is_ocpdiag else "_ocpdiag_test_pkg_%s_launcher" % ocpdiag_test_name

    # Create version file
    version_file = "_%s_version_file" % name
    create_version_file(name = version_file)

    # Create description file
    description_file = "_%s_description_file" % name
    create_description_file(name = description_file, description = description)

    # Create symlink
    binary_path = "%s" % (debian_test_name)
    version_file_path = "version_file.txt"
    package_dir = "%s/" % (native.package_name()) if native.package_name() else ""
    symlinks = {
        binary_path: "%s%s" % (package_dir, binary),
        version_file_path: "%s%s" % (package_dir, version_file),
    }
    if json_defaults:
        json_path = "%s.json" % (debian_test_name)
        symlinks[json_path] = "%s%s" % (package_dir, json_defaults)

    # Build the debian package of different architecture
    config = {}
    for arch in arch_constraints:
        deb_name = "%s_%s" % (name, arch)

        create_ocpdiag_deb(
            name = deb_name,
            test_name = debian_test_name,
            srcs = [":" + binary, ":" + version_file],
            symlinks = symlinks,
            description_file = description_file,
            architecture = arch,
        )

        config_name = "_is_%s_%s" % (name, arch)
        native.config_setting(
            name = config_name,
            constraint_values = arch_constraints[arch],
        )
        config[config_name] = ":%s" % deb_name

    native.alias(
        name = name,
        actual = select(
            config,
            no_match_error = "Please build on x86_64, aarch64 platforms.",
        ),
    )

def create_ocpdiag_deb(
        name,
        test_name,
        srcs,
        symlinks,
        description_file,
        architecture):
    """Create pointer and payload tar archive files and build the debian package.

    Args:
      name: Name of debian package build rule.
      test_name: The name of the OCPDiag test.
      srcs: Target to include in the debian package.
      symlinks: Symlinks to include in the debian package.
      description_file: The package description file.
      architecture: Target architectures for the package.
    """

    # Create the pointer tar archive.
    pointer = "_%s_pointer_tar" % name
    pkg_tar(
        name = pointer,
        extension = "tar.gz",
        visibility = ["//visibility:private"],
    )

    # Create the payload tar archive.
    payload = "_%s_payload_tar" % name
    pkg_tar_payload(
        name = payload,
        test_name = test_name,
        srcs = srcs,
        symlinks = symlinks,
        visibility = ["//visibility:private"],
    )

    # Build the ocpdiag debian pacakge
    pkg_deb_ocpdiag(
        name = name,
        package = test_name,
        pointer_data = pointer,
        payload_data = payload,
        architecture = architecture,
        maintainer = "ocpdiag-core-team@google.com",
        description_file = description_file,
    )

def join_paths(path, *others):
    return paths.normalize(paths.join(path, *others))

def runpath(ctx, file):
    return join_paths(
        ctx.workspace_name,
        file.owner.workspace_root,
        file.short_path,
    )

def valid(items):
    return [item for item in items if item]

def _launcher_gen_impl(ctx):
    template = """#!/bin/sh
    export RUNFILES=${{RUNFILES:-$0.runfiles}}
    exec {launcher} {binary} {descriptor} {defaults} "$@"
    """
    prefix = '"${RUNFILES}/%s"'
    script = template.format(
        launcher = prefix % runpath(ctx, ctx.executable._launcher),
        binary = prefix % runpath(ctx, ctx.executable.binary),
        descriptor = prefix % runpath(ctx, ctx.file.descriptor),
        defaults = prefix % runpath(ctx, ctx.file.defaults) if ctx.attr.defaults else "",
    )
    shfile = ctx.actions.declare_file(ctx.attr.name + ".sh")
    ctx.actions.write(shfile, script, is_executable = True)

    deps = valid([ctx.attr._launcher, ctx.attr.binary, ctx.attr.descriptor, ctx.attr.defaults])
    runfiles = ctx.runfiles(valid([ctx.file.descriptor, ctx.file.defaults]))
    for dep in deps:
        runfiles = runfiles.merge(dep[DefaultInfo].default_runfiles)
    return [DefaultInfo(
        files = depset([shfile]),
        runfiles = runfiles,
    )]

_launcher_gen = rule(
    implementation = _launcher_gen_impl,
    attrs = {
        "binary": attr.label(mandatory = True, executable = True, cfg = "target"),
        "descriptor": attr.label(mandatory = True, allow_single_file = True),
        "defaults": attr.label(allow_single_file = True),
        "_launcher": attr.label(
            default = "//ocpdiag/core/params:ocpdiag_launcher",
            executable = True,
            cfg = "target",
        ),
    },
)

def ocpdiag_launcher(name, binary, descriptors, defaults, **kwargs):
    script = "_%s_ocpdiag_launcher_script" % name
    _launcher_gen(
        name = script,
        binary = binary,
        descriptor = descriptors,
        defaults = defaults,
        visibility = ["//visibility:private"],
    )
    native.sh_binary(
        name = name,
        srcs = [":%s" % script],
        **kwargs
    )

def _create_sar(ctx):
    sar_header = "_ocpdiag_test_pkg_%s_sar_header.sh" % ctx.label.name
    sar_header_file = ctx.actions.declare_file(sar_header)
    sar_contents = """#!/bin/sh -e
RUNFILES=$(
  CMD=$(basename $0)
  DIR=${TMPDIR:-/tmp}
  EXEC=$(mktemp $DIR/$CMD.XXX); chmod a+x $EXEC
  if ! $EXEC >/dev/null 2>&1; then
    DIR=${XDG_RUNTIME_DIR:?"No Executable Run Dir"}
  fi
  rm $EXEC
  mktemp -d $DIR/$CMD.XXX
)
chmod 0755 $RUNFILES
FIFO=$RUNFILES/.running; mkfifo $FIFO
(trap "" INT TERM; cat $FIFO; rm -rf $RUNFILES)&
exec 8>$FIFO 9<&0 <$0
until [ "$l" = "+" ]; do
  read l
done
tar xmz -C $RUNFILES
exec <&9 9<&- $RUNFILES/%s "${@}"
exit
+\n""" % (ctx.executable.exec.basename)
    ctx.actions.write(output = sar_header_file, content = sar_contents)

    sar_output = ctx.actions.declare_file(ctx.label.name)

    if len(ctx.attr.srcs) != 1:
        fail("ocpdiag_self_extracting_archive srcs only accepts one argument in 'srcs', got %s" % len(ctx.attr.srcs))
    tar_file = ctx.attr.srcs[0][DefaultInfo].files.to_list()[0]
    ctx.actions.run_shell(
        inputs = [sar_header_file, tar_file],
        outputs = [sar_output],
        command = "cat %s %s > %s" % (sar_header_file.path, tar_file.path, sar_output.path),
    )

    return DefaultInfo(
        files = depset([sar_output]),
        executable = sar_output,
    )

ocpdiag_self_extracting_archive = rule(
    implementation = _create_sar,
    attrs = {
        "srcs": attr.label_list(
            mandatory = True,
            doc = "Source tarball to package as SAR payload.",
        ),
        "exec": attr.label(
            mandatory = True,
            doc = "Binary rule to execute from the passed tarball",
            executable = True,
            cfg = "target",
        ),
    },
    executable = True,
)

def make_relative_path(src, tgt):
    return ("../" * src.count("/")) + tgt

def _pkg_runfiles_impl(ctx):
    runfiles = ctx.runfiles()
    files = []  # List of depsets, for deduplication.
    binaries = []  # List of executables
    for src in [s[DefaultInfo] for s in ctx.attr.srcs]:
        runfiles = runfiles.merge(src.default_runfiles)
        files.append(src.files)
        if src.files_to_run:
            binaries.append(src.files_to_run.executable)
    runfiles_files = runfiles.files.to_list()
    regular_files = depset(direct = binaries, transitive = files).to_list()
    empty_file = ctx.actions.declare_file(ctx.attr.name + ".empty")
    ctx.actions.write(empty_file, "")

    workspace_prefix = join_paths(ctx.attr.runfiles_prefix, ctx.workspace_name)
    manifest = PackageFilegroupInfo(
        pkg_files = [(files_info, ctx.label) for files_info in [
            # Add runfiles to the local workspace.
            PackageFilesInfo(dest_src_map = {
                join_paths(workspace_prefix, file.owner.workspace_root, file.short_path): file
                for file in runfiles_files
            }),
            # Add regular files to root of the package.
            PackageFilesInfo(dest_src_map = {
                file.basename: file
                for file in regular_files
            }),
            # Add empty runfiles.
            PackageFilesInfo(dest_src_map = {
                join_paths(workspace_prefix, file): empty_file
                for file in runfiles.empty_filenames.to_list()
            }),
        ]],
        # Declare runfiles symlinks
        pkg_symlinks = [(sym_info, ctx.label) for sym_info in [
            PackageSymlinkInfo(
                destination = join_paths(workspace_prefix, sym.path),
                target = make_relative_path(sym.path, sym.target_file.short_path),
            )
            for sym in runfiles.symlinks.to_list()
        ] + [
            # Add top-level symlinks to external repos' runfiles.
            PackageSymlinkInfo(
                destination = join_paths(ctx.attr.runfiles_prefix, name),
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
        DefaultInfo(files = depset([empty_file], transitive = files + [runfiles.files])),
    ]

pkg_tar_runfiles = rule(
    implementation = _pkg_runfiles_impl,
    provides = [PackageFilegroupInfo],
    attrs = {
        "srcs": attr.label_list(
            mandatory = True,
            doc = "Target to include in the tarball.",
        ),
        "runfiles_prefix": attr.string(
            doc = "Path prefix for the runfiles directory",
            default = ".",
        ),
    },
)

def create_ocpdiag_sar(name, launcher, **kwargs):
    """ Bundles a ocpdiag diag into a self-extracting archive.

    Args:
      name:      Name of the ocpdiag_test_pkg, will also be the sar output name
      launcher:  Executable to be bundled into the SAR
      **kwargs:  Any arguments to be forwarded to gentar or the self-extracting archive generator
    """

    # Extract just the executable name, discarding the path.
    launcher_path = _parse_label(launcher)[1]

    runfiles = "_%s_runfiles" % name
    pkg_tar_runfiles(
        name = runfiles,
        srcs = [launcher],
        runfiles_prefix = launcher_path + ".runfiles",
        visibility = ["//visibility:private"],
    )

    tar_name = "_%s_test_tar" % name
    pkg_tar(
        name = tar_name,
        srcs = [":" + runfiles],
        extension = "tgz",
        visibility = ["//visibility:private"],
    )

    ocpdiag_self_extracting_archive(
        name = name,
        srcs = [":" + tar_name],
        exec = launcher,
        **kwargs
    )
