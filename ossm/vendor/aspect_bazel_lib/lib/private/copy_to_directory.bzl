"copy_to_directory implementation"

load(":copy_common.bzl", _COPY_EXECUTION_REQUIREMENTS = "COPY_EXECUTION_REQUIREMENTS")
load(":directory_path.bzl", "DirectoryPathInfo")
load(":paths.bzl", "paths")

_filter_transforms_order_docstring = """Filters and transformations are applied in the following order:

1. `include_external_repositories`

2. `include_srcs_packages`

3. `exclude_srcs_packages`

4. `root_paths`

5. `include_srcs_patterns`

6. `exclude_srcs_patterns`

7. `replace_prefixes`

For more information each filters / transformations applied, see
the documentation for the specific filter / transformation attribute.
"""

_glob_support_docstring = """Glob patterns are supported. Standard wildcards (globbing patterns) plus the `**` doublestar (aka. super-asterisk)
are supported with the underlying globbing library, https://github.com/bmatcuk/doublestar. This is the same
globbing library used by [gazelle](https://github.com/bazelbuild/bazel-gazelle). See https://github.com/bmatcuk/doublestar#patterns
for more information on supported globbing patterns.
"""

_copy_to_directory_doc = """Copies files and directories to an output directory.

Files and directories can be arranged as needed in the output directory using
the `root_paths`, `include_srcs_patterns`, `exclude_srcs_patterns` and `replace_prefixes` attributes.

{filters_transform_order_docstring}

{glob_support_docstring}
""".format(
    filters_transform_order_docstring = _filter_transforms_order_docstring,
    glob_support_docstring = _glob_support_docstring,
)

_copy_to_directory_attr_doc = {
    # srcs
    "srcs": """Files and/or directories or targets that provide `DirectoryPathInfo` to copy into the output directory.""",
    # out
    "out": """Path of the output directory, relative to this package.

If not set, the name of the target is used.
""",
    "add_directory_to_runfiles": """Whether to add the outputted directory to the target's runfiles.""",
    # root_paths
    "root_paths": """List of paths (with glob support) that are roots in the output directory.

If any parent directory of a file being copied matches one of the root paths
patterns specified, the output directory path will be the path relative to the root path
instead of the path relative to the file's workspace. If there are multiple
root paths that match, the longest match wins.

Matching is done on the parent directory of the output file path so a trailing '**' glob patterm
will match only up to the last path segment of the dirname and will not include the basename.
Only complete path segments are matched. Partial matches on the last segment of the root path
are ignored.

Forward slashes (`/`) should be used as path separators.

A `"."` value expands to the target's package path (`ctx.label.package`).

Defaults to `["."]` which results in the output directory path of files in the
target's package and and sub-packages are relative to the target's package and
files outside of that retain their full workspace relative paths.

Globs are supported (see rule docstring above).
""",
    # include_external_repositories
    "include_external_repositories": """List of external repository names (with glob support) to include in the output directory.

Files from external repositories are only copied into the output directory if
the external repository they come from matches one of the external repository patterns
specified or if they are in the same external repository as this target.

When copied from an external repository, the file path in the output directory
defaults to the file's path within the external repository. The external repository
name is _not_ included in that path.

For example, the following copies `@external_repo//path/to:file` to
`path/to/file` within the output directory.

```
copy_to_directory(
    name = "dir",
    include_external_repositories = ["external_*"],
    srcs = ["@external_repo//path/to:file"],
)
```

Files that come from matching external are subject to subsequent filters and
transformations to determine if they are copied and what their path in the output
directory will be. The external repository name of the file from an external
repository is not included in the output directory path and is considered in subsequent
filters and transformations.

Globs are supported (see rule docstring above).
""",
    # include_srcs_packages
    "include_srcs_packages": """List of Bazel packages (with glob support) to include in output directory.

Files in srcs are only copied to the output directory if
the Bazel package of the file matches one of the patterns specified.

Forward slashes (`/`) should be used as path separators. A first character of `"."`
will be replaced by the target's package path.

Defaults to `["**"]` which includes sources from all packages.

Files that have matching Bazel packages are subject to subsequent filters and
transformations to determine if they are copied and what their path in the output
directory will be.

Globs are supported (see rule docstring above).
""",
    # exclude_srcs_packages
    "exclude_srcs_packages": """List of Bazel packages (with glob support) to exclude from output directory.

Files in srcs are not copied to the output directory if
the Bazel package of the file matches one of the patterns specified.

Forward slashes (`/`) should be used as path separators. A first character of `"."`
will be replaced by the target's package path.

Files that have do not have matching Bazel packages are subject to subsequent
filters and transformations to determine if they are copied and what their path in the output
directory will be.

Globs are supported (see rule docstring above).
""",
    # include_srcs_patterns
    "include_srcs_patterns": """List of paths (with glob support) to include in output directory.

Files in srcs are only copied to the output directory if their output
directory path, after applying `root_paths`, matches one of the patterns specified.

Forward slashes (`/`) should be used as path separators.

Defaults to `["**"]` which includes all sources.

Files that have matching output directory paths are subject to subsequent
filters and transformations to determine if they are copied and what their path in the output
directory will be.

Globs are supported (see rule docstring above).
""",
    # exclude_srcs_patterns
    "exclude_srcs_patterns": """List of paths (with glob support) to exclude from output directory.

Files in srcs are not copied to the output directory if their output
directory path, after applying `root_paths`, matches one of the patterns specified.

Forward slashes (`/`) should be used as path separators.

Files that do not have matching output directory paths are subject to subsequent
filters and transformations to determine if they are copied and what their path in the output
directory will be.

Globs are supported (see rule docstring above).
""",
    # replace_prefixes
    "replace_prefixes": """Map of paths prefixes (with glob support) to replace in the output directory path when copying files.

If the output directory path for a file starts with or fully matches a
a key in the dict then the matching portion of the output directory path is
replaced with the dict value for that key. The final path segment
matched can be a partial match of that segment and only the matching portion will
be replaced. If there are multiple keys that match, the longest match wins.

Forward slashes (`/`) should be used as path separators.

Replace prefix transformation are the final step in the list of filters and transformations.
The final output path of a file being copied into the output directory
is determined at this step.

Globs are supported (see rule docstring above).
""",
    # allow_overwrites
    "allow_overwrites": """If True, allow files to be overwritten if the same output file is copied to twice.

The order of srcs matters as the last copy of a particular file will win when overwriting.
Performance of copy_to_directory will be slightly degraded when allow_overwrites is True
since copies cannot be parallelized out as they are calculated. Instead all copy paths
must be calculated before any copies can be started.
""",
    # hardlink
    "hardlink": """Controls when to use hardlinks to files instead of making copies.

Creating hardlinks is much faster than making copies of files with the caveat that
hardlinks share file permissions with their source.

Since Bazel removes write permissions on files in the output tree after an action completes,
hardlinks to source files are not recommended since write permissions will be inadvertently
removed from sources files.

- `auto`: hardlinks are used for generated files already in the output tree
- `off`: all files are copied
- `on`: hardlinks are used for all files (not recommended)
    """,
    "preserve_mtime": """If True, the last modified time of copied files is preserved.
    See the [caveats on copy_directory](/docs/copy_directory.md#preserving-modification-times)
    about interactions with remote execution and caching.""",
    # verbose
    "verbose": """If true, prints out verbose logs to stdout""",
}

_copy_to_directory_attr = {
    "srcs": attr.label_list(
        allow_files = True,
        doc = _copy_to_directory_attr_doc["srcs"],
    ),
    # Cannot declare out as an output here, because there's no API for declaring
    # TreeArtifact outputs.
    "out": attr.string(
        doc = _copy_to_directory_attr_doc["out"],
    ),
    # TODO(3.0): Remove this attribute and do not add directory to runfiles by default.
    # https://github.com/bazel-contrib/bazel-lib/issues/748
    "add_directory_to_runfiles": attr.bool(
        default = True,
        doc = _copy_to_directory_attr_doc["add_directory_to_runfiles"],
    ),
    "root_paths": attr.string_list(
        default = ["."],
        doc = _copy_to_directory_attr_doc["root_paths"],
    ),
    "include_external_repositories": attr.string_list(
        doc = _copy_to_directory_attr_doc["include_external_repositories"],
    ),
    "include_srcs_packages": attr.string_list(
        default = ["**"],
        doc = _copy_to_directory_attr_doc["include_srcs_packages"],
    ),
    "exclude_srcs_packages": attr.string_list(
        doc = _copy_to_directory_attr_doc["exclude_srcs_packages"],
    ),
    "include_srcs_patterns": attr.string_list(
        default = ["**"],
        doc = _copy_to_directory_attr_doc["include_srcs_patterns"],
    ),
    "exclude_srcs_patterns": attr.string_list(
        doc = _copy_to_directory_attr_doc["exclude_srcs_patterns"],
    ),
    "replace_prefixes": attr.string_dict(
        doc = _copy_to_directory_attr_doc["replace_prefixes"],
    ),
    "allow_overwrites": attr.bool(
        doc = _copy_to_directory_attr_doc["allow_overwrites"],
    ),
    "hardlink": attr.string(
        values = ["auto", "off", "on"],
        default = "auto",
        doc = _copy_to_directory_attr_doc["hardlink"],
    ),
    "preserve_mtime": attr.bool(
        default = False,
        doc = _copy_to_directory_attr_doc["preserve_mtime"],
    ),
    "verbose": attr.bool(
        doc = _copy_to_directory_attr_doc["verbose"],
    ),
    # use '_tool' attribute for development only; do not commit with this attribute active since it
    # propagates a dependency on rules_go which would be breaking for users
    # "_tool": attr.label(
    #     executable = True,
    #     cfg = "exec",
    #     default = "//tools/copy_to_directory",
    # ),
}

def _copy_to_directory_impl(ctx):
    copy_to_directory_bin = ctx.toolchains["@aspect_bazel_lib//lib:copy_to_directory_toolchain_type"].copy_to_directory_info.bin

    dst = ctx.actions.declare_directory(ctx.attr.out if ctx.attr.out else ctx.attr.name)

    copy_to_directory_bin_action(
        ctx,
        name = ctx.attr.name,
        dst = dst,
        # copy_to_directory_bin = ctx.executable._tool,  # use for development
        copy_to_directory_bin = copy_to_directory_bin,
        files = ctx.files.srcs,
        targets = [t for t in ctx.attr.srcs if DirectoryPathInfo in t],
        root_paths = ctx.attr.root_paths,
        include_external_repositories = ctx.attr.include_external_repositories,
        include_srcs_packages = ctx.attr.include_srcs_packages,
        exclude_srcs_packages = ctx.attr.exclude_srcs_packages,
        include_srcs_patterns = ctx.attr.include_srcs_patterns,
        exclude_srcs_patterns = ctx.attr.exclude_srcs_patterns,
        replace_prefixes = ctx.attr.replace_prefixes,
        allow_overwrites = ctx.attr.allow_overwrites,
        hardlink = ctx.attr.hardlink,
        preserve_mtime = ctx.attr.preserve_mtime,
        verbose = ctx.attr.verbose,
    )

    runfiles = ctx.runfiles([dst]) if ctx.attr.add_directory_to_runfiles else None

    return [
        DefaultInfo(
            files = depset([dst]),
            runfiles = runfiles,
        ),
    ]

def _expand_src_packages_patterns(patterns, package):
    result = []
    for pattern in patterns:
        if pattern.startswith("."):
            if not package and pattern.startswith("./"):
                # special case in the root package
                result.append(pattern[2:])
            else:
                result.append(package + pattern[1:])
        else:
            result.append(pattern)
    return result

def copy_to_directory_bin_action(
        ctx,
        name,
        dst,
        copy_to_directory_bin,
        copy_to_directory_toolchain = "@aspect_bazel_lib//lib:copy_to_directory_toolchain_type",
        files = [],
        targets = [],
        root_paths = ["."],
        include_external_repositories = [],
        include_srcs_packages = ["**"],
        exclude_srcs_packages = [],
        include_srcs_patterns = ["**"],
        exclude_srcs_patterns = [],
        replace_prefixes = {},
        allow_overwrites = False,
        hardlink = "auto",
        preserve_mtime = False,
        verbose = False):
    """Factory function to copy files to a directory using a tool binary.

    The tool binary will typically be the `@aspect_bazel_lib//tools/copy_to_directory` `go_binary`
    either built from source or provided by a toolchain.

    This helper is used by copy_to_directory. It is exposed as a public API so it can be used within
    other rule implementations where additional_files can also be passed in.

    Args:
        ctx: The rule context.

        name: Name of target creating this action used for config file generation.

        dst: The directory to copy to. Must be a TreeArtifact.

        copy_to_directory_bin: Copy to directory tool binary.

        copy_to_directory_toolchain: The toolchain type for Auto Exec Groups. The default is probably what you want.

        files: List of files to copy into the output directory.

        targets: List of targets that provide `DirectoryPathInfo` to copy into the output directory.

        root_paths: List of paths that are roots in the output directory.

            See copy_to_directory rule documentation for more details.

        include_external_repositories: List of external repository names to include in the output directory.

            See copy_to_directory rule documentation for more details.

        include_srcs_packages: List of Bazel packages to include in output directory.

            See copy_to_directory rule documentation for more details.

        exclude_srcs_packages: List of Bazel packages (with glob support) to exclude from output directory.

            See copy_to_directory rule documentation for more details.

        include_srcs_patterns: List of paths (with glob support) to include in output directory.

            See copy_to_directory rule documentation for more details.

        exclude_srcs_patterns: List of paths (with glob support) to exclude from output directory.

            See copy_to_directory rule documentation for more details.

        replace_prefixes: Map of paths prefixes to replace in the output directory path when copying files.

            See copy_to_directory rule documentation for more details.

        allow_overwrites: If True, allow files to be overwritten if the same output file is copied to twice.

            See copy_to_directory rule documentation for more details.

        hardlink: Controls when to use hardlinks to files instead of making copies.

            See copy_to_directory rule documentation for more details.

        preserve_mtime: If true, preserve the modified time from the source.

        verbose: If true, prints out verbose logs to stdout
    """

    # Replace "." in root_paths with the package name of the target
    root_paths = [p if p != "." else ctx.label.package for p in root_paths]

    # Replace a leading "." with the package name of the target in include_srcs_packages & exclude_srcs_packages
    include_srcs_packages = _expand_src_packages_patterns(include_srcs_packages, ctx.label.package)
    exclude_srcs_packages = _expand_src_packages_patterns(exclude_srcs_packages, ctx.label.package)

    if not include_srcs_packages:
        fail("An empty 'include_srcs_packages' list will exclude all srcs and result in an empty directory")

    if "**" in exclude_srcs_packages:
        fail("A '**' glob pattern in 'exclude_srcs_packages' will exclude all srcs and result in an empty directory")

    if not include_srcs_patterns:
        fail("An empty 'include_srcs_patterns' list will exclude all srcs and result in an empty directory")

    if "**" in exclude_srcs_patterns:
        fail("A '**' glob pattern in 'exclude_srcs_patterns' will exclude all srcs and result in an empty directory")

    for replace_prefix in replace_prefixes.keys():
        if replace_prefix.endswith("**"):
            msg = "replace_prefix '{}' must not end with '**' glob expression".format(replace_prefix)
            fail(msg)

    files_and_targets = []
    for f in files:
        files_and_targets.append(struct(
            file = f,
            path = f.path,
            root_path = f.root.path,
            short_path = f.short_path,
            workspace_path = paths.to_repository_relative_path(f),
        ))
    for t in targets:
        if not DirectoryPathInfo in t:
            continue
        files_and_targets.append(struct(
            file = t[DirectoryPathInfo].directory,
            path = "/".join([t[DirectoryPathInfo].directory.path, t[DirectoryPathInfo].path]),
            root_path = t[DirectoryPathInfo].directory.root.path,
            short_path = "/".join([t[DirectoryPathInfo].directory.short_path, t[DirectoryPathInfo].path]),
            workspace_path = "/".join([paths.to_repository_relative_path(t[DirectoryPathInfo].directory), t[DirectoryPathInfo].path]),
        ))

    file_infos = []
    file_inputs = []
    for f in files_and_targets:
        if not f.file.owner:
            msg = "Expected an owner target label for file {} but found none".format(f)
            fail(msg)

        if f.file.owner.package == None:
            msg = "Expected owner target label for file {} to have a package name but found None".format(f)
            fail(msg)

        if f.file.owner.workspace_name == None:
            msg = "Expected owner target label for file {} to have a workspace name but found None".format(f)
            fail(msg)

        hardlink_file = False
        if hardlink == "on":
            hardlink_file = True
        elif hardlink == "auto":
            hardlink_file = not f.file.is_source

        file_infos.append({
            "package": f.file.owner.package,
            "path": f.path,
            "root_path": f.root_path,
            "short_path": f.short_path,
            "workspace": f.file.owner.workspace_name,
            "workspace_path": f.workspace_path,
            "hardlink": hardlink_file,
        })
        file_inputs.append(f.file)

    config = {
        "allow_overwrites": allow_overwrites,
        "dst": dst.path,
        "exclude_srcs_packages": exclude_srcs_packages,
        "exclude_srcs_patterns": exclude_srcs_patterns,
        "files": file_infos,
        "include_external_repositories": include_external_repositories,
        "include_srcs_packages": include_srcs_packages,
        "include_srcs_patterns": include_srcs_patterns,
        "replace_prefixes": replace_prefixes,
        "root_paths": root_paths,
        "preserve_mtime": preserve_mtime,
        "verbose": verbose,
    }

    config_file = ctx.actions.declare_file("{}_config.json".format(name))
    ctx.actions.write(
        output = config_file,
        content = json.encode_indent(config),
    )

    ctx.actions.run(
        inputs = file_inputs + [config_file],
        outputs = [dst],
        executable = copy_to_directory_bin,
        arguments = [config_file.path, ctx.label.workspace_name],
        mnemonic = "CopyToDirectory",
        progress_message = "Copying files to directory %{output}",
        execution_requirements = _COPY_EXECUTION_REQUIREMENTS,
        toolchain = copy_to_directory_toolchain,
    )

copy_to_directory_lib = struct(
    doc = _copy_to_directory_doc,
    attr_doc = _copy_to_directory_attr_doc,
    attrs = _copy_to_directory_attr,
    impl = _copy_to_directory_impl,
    provides = [DefaultInfo],
)
