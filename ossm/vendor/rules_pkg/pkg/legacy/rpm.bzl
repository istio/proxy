# Copyright 2017 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Rules to create RPM archives.

NOTE: this module is deprecated in favor of pkg/rpm_pfg.bzl. For more
information on the `pkg_filegroup` framework it uses, see pkg/mappings.bzl.

pkg_rpm() depends on the existence of an rpmbuild toolchain. Many users will
find to convenient to use the one provided with their system. To enable that
toolchain add the following stanza to WORKSPACE:

    # Find rpmbuild if it exists.
    load("@rules_pkg//toolchains/rpm:rpmbuild_configure.bzl", "find_system_rpmbuild")
    find_system_rpmbuild(name="rules_pkg_rpmbuild")
"""

rpm_filetype = [".rpm"]

spec_filetype = [".spec"]

def _pkg_rpm_impl(ctx):
    """Implements to pkg_rpm rule."""

    files = []
    tools = []
    args = ["--name=" + ctx.label.name]
    if ctx.attr.debug:
        args.append("--debug")

    if ctx.attr.rpmbuild_path:
        args.append("--rpmbuild=" + ctx.attr.rpmbuild_path)

        # buildifier: disable=print
        print("rpmbuild_path is deprecated. See the README for instructions on how" +
              " to migrate to toolchains")
    else:
        toolchain = ctx.toolchains["@rules_pkg//toolchains/rpm:rpmbuild_toolchain_type"].rpmbuild
        if not toolchain.valid:
            fail("The rpmbuild_toolchain is not properly configured: " +
                 toolchain.name)
        if toolchain.path:
            args.append("--rpmbuild=" + toolchain.path)
        else:
            executable_files = toolchain.label[DefaultInfo].files_to_run
            tools.append(executable_files)
            args.append("--rpmbuild=%s" % executable_files.executable.path)

    # Version can be specified by a file or inlined.
    if ctx.attr.version_file:
        if ctx.attr.version:
            fail("Both version and version_file attributes were specified")
        args.append("--version=@" + ctx.file.version_file.path)
        files.append(ctx.file.version_file)
    elif ctx.attr.version:
        args.append("--version=" + ctx.attr.version)

    # Release can be specified by a file or inlined.
    if ctx.attr.release_file:
        if ctx.attr.release:
            fail("Both release and release_file attributes were specified")
        args.append("--release=@" + ctx.file.release_file.path)
        files.append(ctx.file.release_file)
    elif ctx.attr.release:
        args.append("--release=" + ctx.attr.release)

    # SOURCE_DATE_EPOCH can be specified by a file or inlined.
    if ctx.attr.source_date_epoch_file:
        if ctx.attr.source_date_epoch:
            fail("Both source_date_epoch and source_date_epoch_file attributes were specified")
        args.append("--source_date_epoch=@" + ctx.file.source_date_epoch_file.path)
        files.append(ctx.file.source_date_epoch_file)
    elif ctx.attr.source_date_epoch != None:
        args.append("--source_date_epoch=" + str(ctx.attr.source_date_epoch))

    if ctx.attr.architecture:
        args.append("--arch=" + ctx.attr.architecture)

    if not ctx.attr.spec_file:
        fail("spec_file was not specified")

    # Expand the spec file template.
    spec_file = ctx.actions.declare_file("%s.spec" % ctx.label.name)

    # Create the default substitutions based on the data files.
    substitutions = {}
    for data_file in ctx.files.data:
        key = "{%s}" % data_file.basename
        substitutions[key] = data_file.path
    ctx.actions.expand_template(
        template = ctx.file.spec_file,
        output = spec_file,
        substitutions = substitutions,
    )
    args.append("--spec_file=" + spec_file.path)
    files.append(spec_file)

    args.append("--out_file=" + ctx.outputs.rpm.path)

    # Add data files.
    if ctx.file.changelog:
        files.append(ctx.file.changelog)
        args.append(ctx.file.changelog.path)
    files += ctx.files.data

    for f in ctx.files.data:
        args.append(f.path)

    # Call the generator script.
    ctx.actions.run(
        mnemonic = "MakeRpm",
        executable = ctx.executable._make_rpm,
        use_default_shell_env = True,
        arguments = args,
        inputs = files,
        outputs = [ctx.outputs.rpm],
        env = {
            "LANG": "en_US.UTF-8",
            "LC_CTYPE": "UTF-8",
            "PYTHONIOENCODING": "UTF-8",
            "PYTHONUTF8": "1",
        },
        tools = tools,
    )

    # Link the RPM to the expected output name.
    ctx.actions.symlink(
        output = ctx.outputs.out,
        target_file = ctx.outputs.rpm,
    )

    # Link the RPM to the RPM-recommended output name if possible.
    if "rpm_nvra" in dir(ctx.outputs):
        ctx.actions.symlink(
            output = ctx.outputs.rpm_nvra,
            target_file = ctx.outputs.rpm,
        )

def _pkg_rpm_outputs(version, release):
    outputs = {
        "out": "%{name}.rpm",
        "rpm": "%{name}-%{architecture}.rpm",
    }

    # The "rpm_nvra" output follows the recommended package naming convention of
    # Name-Version-Release.Arch.rpm
    # See http://ftp.rpm.org/max-rpm/ch-rpm-file-format.html
    if version and release:
        outputs["rpm_nvra"] = "%{name}-%{version}-%{release}.%{architecture}.rpm"

    return outputs

# Define the rule.
pkg_rpm = rule(
    doc = "Legacy version",
    attrs = {
        "spec_file": attr.label(
            mandatory = True,
            allow_single_file = spec_filetype,
        ),
        "architecture": attr.string(default = "all"),
        "version_file": attr.label(
            allow_single_file = True,
        ),
        "version": attr.string(),
        "changelog": attr.label(
            allow_single_file = True,
        ),
        "data": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "release_file": attr.label(allow_single_file = True),
        "release": attr.string(),
        "source_date_epoch_file": attr.label(allow_single_file = True),
        "source_date_epoch": attr.int(),
        "debug": attr.bool(default = False),

        # Implicit dependencies.
        "rpmbuild_path": attr.string(),  # deprecated
        "_make_rpm": attr.label(
            default = Label("//pkg:make_rpm"),
            cfg = "exec",
            executable = True,
            allow_files = True,
        ),
    },
    executable = False,
    outputs = _pkg_rpm_outputs,
    implementation = _pkg_rpm_impl,
    toolchains = ["@rules_pkg//toolchains/rpm:rpmbuild_toolchain_type"],
)

# buildifier: disable=no-effect
"""Creates an RPM format package from the data files.

This runs rpmbuild (and requires it to be installed beforehand) to generate
an RPM package based on the spec_file and data attributes.

Two outputs are guaranteed to be produced: "%{name}.rpm", and
"%{name}-%{architecture}.rpm". If the "version" and "release" arguments are
non-empty, a third output will be produced, following the RPM-recommended
N-V-R.A format (Name-Version-Release.Architecture.rpm). Note that due to
the fact that rule implementations cannot access the contents of files,
the "version_file" and "release_file" arguments will not create an output
using N-V-R.A format.

Args:
  spec_file: The RPM spec file to use. If the version or version_file
    attributes are provided, the Version in the spec will be overwritten,
    and likewise behaviour with release and release_file. Any Sources listed
    in the spec file must be provided as data dependencies.
    The base names of data dependencies can be replaced with the actual location
    using "{basename}" syntax.
  version: The version of the package to generate. This will overwrite any
    Version provided in the spec file. Only specify one of version and
    version_file.
  version_file: A file containing the version of the package to generate. This
    will overwrite any Version provided in the spec file. Only specify one of
    version and version_file.
  release: The release of the package to generate. This will overwrite any
    release provided in the spec file. Only specify one of release and
    release_file.
  release_file: A file containing the release of the package to generate. This
    will overwrite any release provided in the spec file. Only specify one of
    release and release_file.
  changelog: A changelog file to include. This will not be written to the spec
    file, which should only list changes to the packaging, not the software itself.
  source_date_epoch: Value to export as SOURCE_DATE_EPOCH to facilitate reproducible
    timestamps.  Implicitly sets the `%clamp_mtime_to_source_date_epoch` in the
    subordinate call to `rpmbuild` to facilitate more consistent in-RPM file
    timestamps.
  source_date_epoch_file: File containing the SOURCE_DATE_EPOCH value.  Sets
    `%clamp_mtime_to_source_date_epoch` like with "source_date_epoch".
  data: List all files to be included in the package here.
"""
