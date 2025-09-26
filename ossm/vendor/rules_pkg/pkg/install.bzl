# Copyright 2021 The Bazel Authors. All rights reserved.
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
"""Rules for creating install scripts from pkg_filegroups and friends.

This module provides an interface (`pkg_install`) for creating a `bazel
run`-able installation script.
"""

load("@rules_python//python:defs.bzl", "py_binary")
load("//pkg:providers.bzl", "PackageDirsInfo", "PackageFilegroupInfo", "PackageFilesInfo", "PackageSymlinkInfo")
load("//pkg/private:pkg_files.bzl", "create_mapping_context_from_ctx", "process_src", "write_manifest")

def _pkg_install_script_impl(ctx):
    script_file = ctx.actions.declare_file(ctx.attr.name + ".py")

    mapping_context = create_mapping_context_from_ctx(ctx, label = ctx.label, default_mode = "0644")
    for src in ctx.attr.srcs:
        process_src(
            mapping_context,
            src = src,
            origin = src.label,
        )

    manifest_file = ctx.actions.declare_file(ctx.attr.name + "-install-manifest.json")

    # Write out the manifest in terms of "short" paths, which are those expected
    # when you make `bazel run`nable binaries).
    #
    # Note that these paths are different when used as tools run within a build.
    # See also
    # https://docs.bazel.build/versions/4.1.0/skylark/rules.html#tools-with-runfiles
    write_manifest(ctx, manifest_file, mapping_context.content_map, use_short_path = True)

    # Get the label of the actual py_binary used to run this script.
    #
    # This is super brittle, but I don't know how to otherwise get this
    # information without creating a circular dependency given the current state
    # of rules_python.

    # The name of the binary is the name of this target, minus
    # "_install_script".
    label_str = str(ctx.label)[:-len("_install_script")]

    ctx.actions.expand_template(
        template = ctx.file._script_template,
        output = script_file,
        substitutions = {
            "{MANIFEST_INCLUSION}": manifest_file.short_path,
            # This is used to extend the manifest paths when the script is run
            # inside a build.
            "{WORKSPACE_NAME}": ctx.workspace_name,
            # Used to annotate --help with "bazel run //path/to/your:installer"
            "{TARGET_LABEL}": label_str,
            "{DEFAULT_DESTDIR}": ctx.attr.destdir,
        },
        is_executable = True,
    )

    my_runfiles = ctx.runfiles(
        files = [manifest_file],
        transitive_files = depset(transitive = mapping_context.file_deps),
    )

    return [
        DefaultInfo(
            files = depset([script_file]),
            runfiles = my_runfiles,
            executable = script_file,
        ),
    ]

_pkg_install_script = rule(
    doc = """Create an executable package installation script.

    The outputs of this rule are a single python script intended to be used as
    an input to a `py_binary` target.  All files necessary to run the script are
    included as runfiles.
    """,
    implementation = _pkg_install_script_impl,
    attrs = {
        "srcs": attr.label_list(
            mandatory = True,
            providers = [
                [PackageFilegroupInfo],
                [PackageFilesInfo],
                [PackageDirsInfo],
                [PackageSymlinkInfo],
            ],
            doc = "Source mapping/grouping targets",
        ),
        "destdir": attr.string(),
        # This is private for now -- one could perhaps imagine making this
        # public, but that would require more documentation of the underlying
        # scripts and expected interfaces.
        "_script_template": attr.label(
            allow_single_file = True,
            default = "//pkg/private:install.py.tpl",
        ),
    },
    executable = True,
)

def pkg_install(name, srcs, destdir = None, **kwargs):
    """Create an installer script from pkg_filegroups and friends.

    This macro allows users to create `bazel run`nable installation scripts
    using the pkg_filegroup framework.

    For example:

    ```python
    pkg_install(
        name = "install",
        srcs = [
            # mapping/grouping targets here
        ],
        destdir = "out/install",
    )
    ```

    Installation can be done by invoking:

    ```
    bazel run -- //path/to:install
    ```

    Additional features can be accessed by invoking the script with the --help
    option:

    ```
    bazel run -- //path/to:install --help
    ```

    WARNING: While this rule does function when being run from within a bazel
    rule, such use is not recommended.  If you do, **always** use the
    `--destdir` argument to specify the desired location for the installation to
    occur.  Not doing so can lead the outputs going to an unexpected location,
    or in some cases, failing.  Run the script command with `--help`, as
    mentioned above, for more details.

    One such use would be to run the script created by `pkg_install` to produce
    a directory output in the build root.  This may not function as expected or
    may suffer from poorly tested edge cases.  A purpose-written rule that would
    allow for creation of such directories is discussed in
    https://github.com/bazelbuild/rules_pkg/issues/388.

    Args:
        name: rule name
        srcs: pkg_filegroup framework mapping or grouping targets
        destdir: The default destination directory.

            If it is specified, this is the default destination to install
            the files. It is overridable by explicitly specifying `--destdir`
            in the command line or specifying the `DESTDIR` environment
            variable.

            If it is not specified, `--destdir` must be set on the command line,
            or the `DESTDIR` environment variable must be set.

            If this is an absolute path, it is used as-is. If this is a relative
            path, it is interpreted against `BUILD_WORKSPACE_DIRECTORY`.
        **kwargs: common rule attributes

    """

    _pkg_install_script(
        name = name + "_install_script",
        srcs = srcs,
        destdir = destdir,
        **kwargs
    )

    py_binary(
        name = name,
        srcs = [":" + name + "_install_script"],
        main = name + "_install_script.py",
        deps = [Label("//pkg/private:manifest")],
        srcs_version = "PY3",
        python_version = "PY3",
        **kwargs
    )
