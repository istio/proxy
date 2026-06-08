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

"""Package creation helper mapping rules.

This module declares Provider interfaces and rules for specifying the contents
of packages in a package-type-agnostic way.  The main rules supported here are
the following:

- `pkg_files` describes destinations for rule outputs
- `pkg_mkdirs` describes directory structures
- `pkg_mklink` describes symbolic links
- `pkg_filegroup` creates groupings of above to add to packages

Rules that actually make use of the outputs of the above rules are not specified
here.
"""

load("@bazel_skylib//lib:paths.bzl", "paths")
load("//pkg:providers.bzl", "PackageDirsInfo", "PackageFilegroupInfo", "PackageFilesInfo", "PackageSymlinkInfo")
load("//pkg/private:util.bzl", "get_repo_mapping_manifest")

# TODO(#333): strip_prefix module functions should produce unique outputs. In
# particular, this one and `_sp_from_pkg` can overlap.
_PKGFILEGROUP_STRIP_ALL = "."

REMOVE_BASE_DIRECTORY = "\0"

def _sp_files_only():
    return _PKGFILEGROUP_STRIP_ALL

def _sp_from_pkg(path = ""):
    if path.startswith("/"):
        return path[1:]
    return path

def _sp_from_root(path = ""):
    if path.startswith("/"):
        return path
    return "/" + path

strip_prefix = struct(
    _doc = """pkg_files `strip_prefix` helper.  Instructs `pkg_files` what to do with directory prefixes of files.

    Each member is a function that equates to:

    - `files_only()`: strip all directory components from all paths

    - `from_pkg(path)`: strip all directory components up to the current
      package, plus what's in `path`, if provided.

    - `from_root(path)`: strip beginning from the file's WORKSPACE root (even if
      it is in an external workspace) plus what's in `path`, if provided.

    Prefix stripping is applied to each `src` in a `pkg_files` rule
    independently.
 """,
    files_only = _sp_files_only,
    from_pkg = _sp_from_pkg,
    from_root = _sp_from_root,
)

def pkg_attributes(
        mode = None,
        user = None,
        group = None,
        uid = None,
        gid = None,
        **kwargs):
    """Format attributes for use in package mapping rules.

    If "mode" is not provided, it will default to the mapping rule's default
    mode.  These vary per mapping rule; consult the respective documentation for
    more details.

    Not providing any of "user", "group", "uid", or "gid" will result in the package
    builder choosing one for you.  The chosen value should not be relied upon.

    Well-known attributes outside of the above are documented in the rules_pkg
    reference.

    This is the only supported means of passing in attributes to package mapping
    rules (e.g. `pkg_files`).

    Args:
      mode: string: UNIXy octal permissions, as a string.
      user: string: Filesystem owning user name.
      group: string: Filesystem owning group name.
      uid: int: Filesystem owning user id.
      gid: int: Filesystem owning group id.
      **kwargs: any other desired attributes.

    Returns:
      A value usable in the "attributes" attribute in package mapping rules.

    """
    ret = kwargs
    if mode:
        ret["mode"] = mode
    if user:
        ret["user"] = user
    if group:
        ret["group"] = group
    if uid != None:
        if type(uid) != type(0):
            fail('Got "' + str(uid) + '" instead of integer uid')
        ret["uid"] = uid
    if gid != None:
        if type(gid) != type(0):
            fail('Got "' + str(gid) + '" instead of integer gid')
        ret["gid"] = gid

    if user != None and user.isdigit() and uid == None:
        # buildifier: disable=print
        print("Warning: found numeric username and no uid, did you mean to specify the uid instead?")

    if group != None and group.isdigit() and gid == None:
        # buildifier: disable=print
        print("Warning: found numeric group and no gid, did you mean to specify the gid instead?")

    return json.encode(ret)

####
# Internal helpers
####

def _do_strip_prefix(path, to_strip, src_file):
    if to_strip == "":
        # We were asked to strip nothing, which is valid.  Just return the
        # original path.
        return path

    path_norm = paths.normalize(path)
    to_strip_norm = paths.normalize(to_strip) + "/"

    if path_norm.startswith(to_strip_norm):
        return path_norm[len(to_strip_norm):]
    if src_file.is_directory and (path_norm + "/") == to_strip_norm:
        return ""

    # Avoid user surprise by failing if prefix stripping doesn't work as expected.
    #
    # We already leave enough breadcrumbs, so if File.owner() returns None,
    # this won't be a problem.
    failmsg = "Could not strip prefix '{}' from file {} ({})".format(to_strip, str(src_file), str(src_file.owner))
    if src_file.is_directory:
        failmsg += """\n\nNOTE: prefix stripping does not operate within TreeArtifacts (directory outputs)

To strip the directory named by the TreeArtifact itself, see documentation for the `renames` attribute.
"""
    fail(failmsg)

# The below routines make use of some path checking magic that may difficult to
# understand out of the box.  This following table may be helpful to demonstrate
# how some of these members may look like in real-world usage:
#
# Note: "F" is "File", "FO": is "File.owner".

# | File type | Repo     | `F.path`                                                 | `F.root.path`                | `F.short_path`          | `FO.workspace_name` | `FO.workspace_root` |
# |-----------|----------|----------------------------------------------------------|------------------------------|-------------------------|---------------------|---------------------|
# | Source    | Local    | `dirA/fooA`                                              |                              | `dirA/fooA`             |                     |                     |
# | Generated | Local    | `bazel-out/k8-fastbuild/bin/dirA/gen.out`                | `bazel-out/k8-fastbuild/bin` | `dirA/gen.out`          |                     |                     |
# | Source    | External | `external/repo2/dirA/fooA`                               |                              | `../repo2/dirA/fooA`    | `repo2`             | `external/repo2`    |
# | Generated | External | `bazel-out/k8-fastbuild/bin/external/repo2/dirA/gen.out` | `bazel-out/k8-fastbuild/bin` | `../repo2/dirA/gen.out` | `repo2`             | `external/repo2`    |

def _owner(file):
    # File.owner allows us to find a label associated with a file.  While highly
    # convenient, it may return None in certain circumstances, which seem to be
    # primarily when bazel doesn't know about the files in question.
    #
    # Given that a sizeable amount of the code we have here relies on it, we
    # should fail() when we encounter this if only to make the rare error more
    # clear.
    #
    # File.owner returns a Label structure
    if file.owner == None:
        fail("File {} ({}) has no owner attribute; cannot continue".format(file, file.path))
    return file.owner

def _relative_workspace_root(label):
    # Helper function that returns the workspace root relative to the bazel File
    # "short_path", so we can exclude external workspace names in the common
    # path stripping logic.
    #
    # This currently is "../$LABEL_WORKSPACE_ROOT" if the label has a specific
    # workspace name specified, else it's just an empty string.
    #
    # TODO(nacl): Make this not a hack
    return paths.join("..", label.workspace_name) if label.workspace_name else ""

def _path_relative_to_package(file):
    # Helper function that returns a path to a file relative to its package.
    owner = _owner(file)
    return paths.relativize(
        file.short_path,
        paths.join(_relative_workspace_root(owner), owner.package),
    )

def _path_relative_to_repo_root(file):
    # Helper function that returns a path to a file relative to its workspace root.
    return paths.relativize(
        file.short_path,
        _relative_workspace_root(_owner(file)),
    )

def _pkg_files_impl(ctx):
    # The input sources are already known. Let's calculate the destinations...

    # Exclude excludes
    srcs = []  # srcs is source File objects, not Targets
    file_to_target = {}
    for src in ctx.attr.srcs:
        for f in src[DefaultInfo].files.to_list():
            if f not in ctx.files.excludes:
                srcs.append(f)
                file_to_target[f] = src

    if ctx.attr.strip_prefix == _PKGFILEGROUP_STRIP_ALL:
        src_dest_paths_map = {src: paths.join(ctx.attr.prefix, src.basename) for src in srcs}
    elif ctx.attr.strip_prefix.startswith("/"):
        # Relative to workspace/repository root
        src_dest_paths_map = {src: paths.join(
            ctx.attr.prefix,
            _do_strip_prefix(
                _path_relative_to_repo_root(src),
                ctx.attr.strip_prefix[1:],
                src,
            ),
        ) for src in srcs}
    else:
        # Relative to package
        src_dest_paths_map = {src: paths.join(
            ctx.attr.prefix,
            _do_strip_prefix(
                _path_relative_to_package(src),
                ctx.attr.strip_prefix,
                src,
            ),
        ) for src in srcs}

    out_attributes = json.decode(ctx.attr.attributes)

    # The least surprising default mode is that of a normal file (0644)
    out_attributes.setdefault("mode", "0644")

    # Do file renaming
    for rename_src, rename_dest in ctx.attr.renames.items():
        # rename_src.files is a depset
        rename_src_files = rename_src[DefaultInfo].files.to_list()

        # Need to do a length check before proceeding. We cannot rename
        # multiple files simultaneously.
        if len(rename_src_files) != 1:
            fail(
                "Target {} expands to multiple files, should only refer to one".format(rename_src),
                "renames",
            )

        src_file = rename_src_files[0]
        if src_file not in src_dest_paths_map:
            fail(
                "File remapping from {0} to {1} is invalid: {0} is not provided to this rule or was excluded".format(rename_src, rename_dest),
                "renames",
            )

        if rename_dest == REMOVE_BASE_DIRECTORY:
            if not src_file.is_directory:
                fail(
                    "REMOVE_BASE_DIRECTORY as a renaming target for non-directories is disallowed.",
                    "renames",
                )

            # REMOVE_BASE_DIRECTORY results in the contents being dropped into
            # place directly in the prefix path.
            src_dest_paths_map[src_file] = ctx.attr.prefix

        else:
            src_dest_paths_map[src_file] = paths.join(ctx.attr.prefix, rename_dest)

    # At this point, we have a fully valid src -> dest mapping for all the
    # explicitly named targets in srcs. Now we can fill in their runfiles.
    if ctx.attr.include_runfiles:
        for src in srcs:
            target = file_to_target[src]
            runfiles = target[DefaultInfo].default_runfiles
            if runfiles:
                base_path = src_dest_paths_map[src] + ".runfiles/" + ctx.workspace_name
                for rf in runfiles.files.to_list():
                    dest_path = paths.join(base_path, rf.short_path)

                    # print("Add runfile:", rf.path, 'as', dest_path)
                    have_it = src_dest_paths_map.get(rf)
                    if have_it:
                        if have_it != dest_path:
                            # buildifier: disable=print
                            print("same source mapped to different locations", rf, have_it, dest_path)
                    else:
                        src_dest_paths_map[rf] = dest_path

                # if repo_mapping manifest exists (for e.g. with --enable_bzlmod),
                # create _repo_mapping under runfiles directory
                repo_mapping_manifest = get_repo_mapping_manifest(target)
                if repo_mapping_manifest:
                    dest_path = paths.join(src_dest_paths_map[src] + ".runfiles", "_repo_mapping")
                    src_dest_paths_map[repo_mapping_manifest] = dest_path

    # At this point, we have a fully valid src -> dest mapping in src_dest_paths_map.
    #
    # Construct the inverse of this mapping to pass to the output providers, and
    # check for duplicated destinations.
    dest_src_map = {}
    for src, dest in src_dest_paths_map.items():
        if dest in dest_src_map:
            fail("After renames, multiple sources (at least {0}, {1}) map to the same destination.  Consider adjusting strip_prefix and/or renames".format(dest_src_map[dest].path, src.path))
        dest_src_map[dest] = src

    return [
        PackageFilesInfo(
            dest_src_map = dest_src_map,
            attributes = out_attributes,
        ),
        DefaultInfo(
            # Simple passthrough
            files = depset(dest_src_map.values()),
        ),
    ]

pkg_files = rule(
    doc = """General-purpose package target-to-destination mapping rule.

    This rule provides a specification for the locations and attributes of
    targets when they are packaged. No outputs are created other than Providers
    that are intended to be consumed by other packaging rules, such as
    `pkg_rpm`. `pkg_files` targets may be consumed by other `pkg_files` or
    `pkg_filegroup` to build up complex layouts, or directly by top level
    packaging rules such as `pkg_files`.

    Consumers of `pkg_files`s will, where possible, create the necessary
    directory structure for your files so you do not have to unless you have
    special requirements.  Consult `pkg_mkdirs` for more details.
    """,
    implementation = _pkg_files_impl,
    # @unsorted-dict-items
    attrs = {
        "srcs": attr.label_list(
            doc = """Files/Labels to include in the outputs of these rules""",
            mandatory = True,
            allow_files = True,
        ),
        "attributes": attr.string(
            doc = """Attributes to set on packaged files.

            Always use `pkg_attributes()` to set this rule attribute.

            If not otherwise overridden, the file's mode will be set to UNIX
            "0644", or the target platform's equivalent.

            Consult the "Mapping Attributes" documentation in the rules_pkg
            reference for more details.
            """,
            default = "{}",  # Empty JSON
        ),
        "prefix": attr.string(
            doc = """Installation prefix.

            This may be an arbitrary string, but it should be understandable by
            the packaging system you are using to have the desired outcome.  For
            example, RPM macros like `%{_libdir}` may work correctly in paths
            for RPM packages, not, say, Debian packages.

            If any part of the directory structure of the computed destination
            of a file provided to `pkg_filegroup` or any similar rule does not
            already exist within a package, the package builder will create it
            for you with a reasonable set of default permissions (typically
            `0755 root.root`).

            It is possible to establish directory structures with arbitrary
            permissions using `pkg_mkdirs`.
            """,
            default = "",
        ),
        "strip_prefix": attr.string(
            doc = """What prefix of a file's path to discard prior to installation.

            This specifies what prefix of an incoming file's path should not be
            included in the output package at after being appended to the
            install prefix (the `prefix` attribute).  Note that this is only
            applied to full directory names, see `strip_prefix` for more
            details.

            Use the `strip_prefix` struct to define this attribute.  If this
            attribute is not specified, all directories will be stripped from
            all files prior to being included in packages
            (`strip_prefix.files_only()`).

            If prefix stripping fails on any file provided in `srcs`, the build
            will fail.

            Note that this only functions on paths that are known at analysis
            time.  Specifically, this will not consider directories within
            TreeArtifacts (directory outputs), or the directories themselves.
            See also #269.
            """,
            default = strip_prefix.files_only(),
        ),
        "excludes": attr.label_list(
            doc = """List of files or labels to exclude from the inputs to this rule.

            Mostly useful for removing files from generated outputs or
            preexisting `filegroup`s.
            """,
            default = [],
            allow_files = True,
        ),
        "renames": attr.label_keyed_string_dict(
            doc = """Destination override map.

            This attribute allows the user to override destinations of files in
            `pkg_file`s relative to the `prefix` attribute.  Keys to the
            dict are source files/labels, values are destinations relative to
            the `prefix`, ignoring whatever value was provided for
            `strip_prefix`.

            If the key refers to a TreeArtifact (directory output), you may
            specify the constant `REMOVE_BASE_DIRECTORY` as the value, which
            will result in all containing files and directories being installed
            relative to the otherwise specified install prefix (via the `prefix`
            and `strip_prefix` attributes), not the directory name.

            The following keys are rejected:

            - Any label that expands to more than one file (mappings must be
              one-to-one).

            - Any label or file that was either not provided or explicitly
              `exclude`d.

            The following values result in undefined behavior:

            - "" (the empty string)

            - "."

            - Anything containing ".."

            """,
            default = {},
            allow_files = True,
        ),
        "include_runfiles": attr.bool(
            doc = """Add runfiles for all srcs.

            The runfiles are in the paths that Bazel uses. For example, for the
            target `//my_prog:foo`, we would see files under paths like
            `foo.runfiles/<repo name>/my_prog/<file>`
            """,
        ),
    },
    provides = [PackageFilesInfo],
)

def _pkg_mkdirs_impl(ctx):
    out_attributes = json.decode(ctx.attr.attributes)

    # The least surprising default mode is that of a normal directory (0755)
    out_attributes.setdefault("mode", "0755")
    return [
        PackageDirsInfo(
            dirs = ctx.attr.dirs,
            attributes = out_attributes,
        ),
    ]

pkg_mkdirs = rule(
    doc = """Defines creation and ownership of directories in packages

    Use this if:

    1) You need to create an empty directory in your package.

    2) Your package needs to explicitly own a directory, even if it already owns
       files in those directories.

    3) You need nonstandard permissions (typically, not "0755") on a directory
       in your package.

    For some package management systems (e.g. RPM), directory ownership (2) may
    imply additional semantics.  Consult your package manager's and target
    distribution's documentation for more details.
    """,
    implementation = _pkg_mkdirs_impl,
    # @unsorted-dict-items
    attrs = {
        "dirs": attr.string_list(
            doc = """Directory names to make within the package

            If any part of the requested directory structure does not already
            exist within a package, the package builder will create it for you
            with a reasonable set of default permissions (typically `0755
            root.root`).

            """,
            mandatory = True,
        ),
        "attributes": attr.string(
            doc = """Attributes to set on packaged directories.

            Always use `pkg_attributes()` to set this rule attribute.

            If not otherwise overridden, the directory's mode will be set to
            UNIX "0755", or the target platform's equivalent.

            Consult the "Mapping Attributes" documentation in the rules_pkg
            reference for more details.
            """,
            default = "{}",  # Empty JSON
        ),
    },
    provides = [PackageDirsInfo],
)

def _pkg_mklink_impl(ctx):
    out_attributes = json.decode(ctx.attr.attributes)

    # The least surprising default mode is that of a symbolic link (0777).
    # Permissions on symlinks typically don't matter, as the operation is
    # typically moved to where the link is pointing.
    out_attributes.setdefault("mode", "0777")
    return [
        PackageSymlinkInfo(
            destination = ctx.attr.link_name,
            target = ctx.attr.target,
            attributes = out_attributes,
        ),
    ]

pkg_mklink_impl = rule(
    doc = """Define a symlink  within packages

    This rule results in the creation of a single link within a package.

    Symbolic links specified by this rule may point at files/directories outside of the
    package, or otherwise left dangling.

    """,
    implementation = _pkg_mklink_impl,
    # @unsorted-dict-items
    attrs = {
        "target": attr.string(
            doc = """Link "target", a path on the filesystem.

            This is what the link "points" to, and may point to an arbitrary
            filesystem path, even relative paths.

            """,
            mandatory = True,
        ),
        "link_name": attr.string(
            doc = """Link "destination", a path within the package.

            This is the actual created symbolic link.

            If the directory structure provided by this attribute is not
            otherwise created when exist within the package when it is built, it
            will be created implicitly, much like with `pkg_files`.

            This path may be prefixed or rooted by grouping or packaging rules.

            """,
            mandatory = True,
        ),
        "attributes": attr.string(
            doc = """Attributes to set on packaged symbolic links.

            Always use `pkg_attributes()` to set this rule attribute.

            Symlink permissions may have different meanings depending on your
            host operating system; consult its documentation for more details.

            If not otherwise overridden, the link's mode will be set to UNIX
            "0777", or the target platform's equivalent.

            Consult the "Mapping Attributes" documentation in the rules_pkg
            reference for more details.
            """,
            default = "{}",  # Empty JSON
        ),
    },
    provides = [PackageSymlinkInfo],
)

#buildifier: disable=function-docstring-args
def pkg_mklink(name, link_name, target, attributes = None, src = None, **kwargs):
    """Create a symlink.

    Wraps [pkg_mklink_impl](#pkg_mklink_impl)

    Args:
      name: target name
      target: target path that the link should point to.
      link_name: the path in the package that should point to the target.
      attributes: file attributes.
    """
    if src:
        if target:
            fail("You can not specify both target and src.")

        # buildifier: disable=print
        print("Warning: pkg_mklink.src is deprecated. Use target.")
        target = src
    pkg_mklink_impl(
        name = name,
        target = target,
        link_name = link_name,
        attributes = attributes,
        **kwargs
    )

def _pkg_filegroup_impl(ctx):
    files = []
    dirs = []
    links = []
    mapped_files_depsets = []

    if ctx.attr.prefix:
        # If "prefix" is provided, we need to manipulate the incoming providers.
        for s in ctx.attr.srcs:
            if PackageFilegroupInfo in s:
                old_pfgi, old_di = s[PackageFilegroupInfo], s[DefaultInfo]

                files += [
                    (
                        PackageFilesInfo(
                            dest_src_map = {
                                paths.join(ctx.attr.prefix, dest): src
                                for dest, src in pfi.dest_src_map.items()
                            },
                            attributes = pfi.attributes,
                        ),
                        origin,
                    )
                    for (pfi, origin) in old_pfgi.pkg_files
                ]
                dirs += [
                    (
                        PackageDirsInfo(
                            dirs = [paths.join(ctx.attr.prefix, d) for d in pdi.dirs],
                            attributes = pdi.attributes,
                        ),
                        origin,
                    )
                    for (pdi, origin) in old_pfgi.pkg_dirs
                ]
                links += [
                    (
                        PackageSymlinkInfo(
                            target = psi.target,
                            destination = paths.join(ctx.attr.prefix, psi.destination),
                            attributes = psi.attributes,
                        ),
                        origin,
                    )
                    for (psi, origin) in old_pfgi.pkg_symlinks
                ]

                mapped_files_depsets.append(old_di.files)

            if PackageFilesInfo in s:
                new_pfi = PackageFilesInfo(
                    dest_src_map = {
                        paths.join(ctx.attr.prefix, dest): src
                        for dest, src in s[PackageFilesInfo].dest_src_map.items()
                    },
                    attributes = s[PackageFilesInfo].attributes,
                )
                files.append((new_pfi, s.label))

                # dict.values() returns a list, not an iterator like in python3
                mapped_files_depsets.append(s[DefaultInfo].files)

            if PackageDirsInfo in s:
                new_pdi = PackageDirsInfo(
                    dirs = [paths.join(ctx.attr.prefix, d) for d in s[PackageDirsInfo].dirs],
                    attributes = s[PackageDirsInfo].attributes,
                )
                dirs.append((new_pdi, s.label))

            if PackageSymlinkInfo in s:
                new_psi = PackageSymlinkInfo(
                    target = s[PackageSymlinkInfo].target,
                    destination = paths.join(ctx.attr.prefix, s[PackageSymlinkInfo].destination),
                    attributes = s[PackageSymlinkInfo].attributes,
                )
                links.append((new_psi, s.label))
    else:
        # Otherwise, everything is pretty much direct copies
        for s in ctx.attr.srcs:
            if PackageFilegroupInfo in s:
                files += s[PackageFilegroupInfo].pkg_files
                mapped_files_depsets.append(s[DefaultInfo].files)
                dirs += s[PackageFilegroupInfo].pkg_dirs
                links += s[PackageFilegroupInfo].pkg_symlinks

            if PackageFilesInfo in s:
                files.append((s[PackageFilesInfo], s.label))

                # dict.values() returns a list, not an iterator like in python3
                mapped_files_depsets.append(s[DefaultInfo].files)
            if PackageDirsInfo in s:
                dirs.append((s[PackageDirsInfo], s.label))
            if PackageSymlinkInfo in s:
                links.append((s[PackageSymlinkInfo], s.label))

    return [
        PackageFilegroupInfo(
            pkg_files = files,
            pkg_dirs = dirs,
            pkg_symlinks = links,
        ),
        # Necessary to ensure that dependent rules have access to files being
        # mapped in.
        DefaultInfo(
            files = depset(transitive = mapped_files_depsets),
        ),
    ]

pkg_filegroup = rule(
    doc = """Package contents grouping rule.

    This rule represents a collection of packaging specifications (e.g. those
    created by `pkg_files`, `pkg_mklink`, etc.) that have something in common,
    such as a prefix or a human-readable category.
    """,
    implementation = _pkg_filegroup_impl,
    # @unsorted-dict-items
    attrs = {
        "srcs": attr.label_list(
            doc = """A list of packaging specifications to be grouped together.""",
            mandatory = True,
            providers = [
                [PackageFilegroupInfo, DefaultInfo],
                [PackageFilesInfo, DefaultInfo],
                [PackageDirsInfo],
                [PackageSymlinkInfo],
            ],
        ),
        "prefix": attr.string(
            doc = """A prefix to prepend to provided paths, applied like so:

            - For files and directories, this is simply prepended to the destination
            - For symbolic links, this is prepended to the "destination" part.

            """,
        ),
    },
    provides = [PackageFilegroupInfo],
)

def _filter_directory_argify_pair(pair):
    return "{}={}".format(*pair)

def _filter_directory_impl(ctx):
    out_dir = ctx.actions.declare_directory(ctx.attr.outdir_name or ctx.attr.name)

    if not ctx.file.src.is_directory:
        fail("Must be a directory (TreeArtifact)", "src")

    args = ctx.actions.args()

    # Flags
    args.add_all(ctx.attr.excludes, before_each = "--exclude")
    args.add_all(ctx.attr.renames.items(), before_each = "--rename", map_each = _filter_directory_argify_pair)

    args.add("--prefix", ctx.attr.prefix)
    args.add("--strip_prefix", ctx.attr.strip_prefix)

    # Adding the directories directly here requires manually specifying the
    # path.  Bazel will reject simply passing in the File object.
    args.add(ctx.file.src.path)
    args.add(out_dir.path)

    ctx.actions.run(
        executable = ctx.executable._filterer,
        use_default_shell_env = True,
        arguments = [args],
        inputs = [ctx.file.src],
        outputs = [out_dir],
    )

    return [DefaultInfo(files = depset([out_dir]))]

filter_directory = rule(
    doc = """Transform directories (TreeArtifacts) using pkg_filegroup-like semantics.

    Effective order of operations:

    1) Files are `exclude`d
    2) `renames` _or_ `strip_prefix` is applied.
    3) `prefix` is applied

    In particular, if a `rename` applies to an individual file, `strip_prefix`
    will not be applied to that particular file.

    Each non-`rename``d path will look like this:

    ```
    $OUTPUT_DIR/$PREFIX/$FILE_WITHOUT_STRIP_PREFIX
    ```

    Each `rename`d path will look like this:

    ```
    $OUTPUT_DIR/$PREFIX/$FILE_RENAMED
    ```

    If an operation cannot be applied (`strip_prefix`) to any component in the
    directory, or if one is unused (`exclude`, `rename`), the underlying command
    will fail.  See the individual attributes for details.
    """,
    implementation = _filter_directory_impl,
    # @unsorted-dict-items
    attrs = {
        "src": attr.label(
            doc = """Directory (TreeArtifact) to process.""",
            allow_single_file = True,
            mandatory = True,
        ),
        "outdir_name": attr.string(
            doc = """Name of output directory (otherwise defaults to the rule's name)""",
        ),
        "strip_prefix": attr.string(
            doc = """Prefix to remove from all paths in the output directory.

            Must apply to all paths in the directory, even those rename'd.
            """,
        ),
        "prefix": attr.string(
            doc = """Prefix to add to all paths in the output directory.

            This does not include the output directory name, which will be added
            regardless.
            """,
        ),
        "renames": attr.string_dict(
            doc = """Files to rename in the output directory.

            Keys are destinations, values are sources prior to any path
            modifications (e.g. via `prefix` or `strip_prefix`).  Files that are
            `exclude`d must not be renamed.

            This currently only operates on individual files.  `strip_prefix`
            does not apply to them.

            All renames must be used.
            """,
        ),
        "excludes": attr.string_list(
            doc = """Files to exclude from the output directory.

            Each element must refer to an individual file in `src`.

            All exclusions must be used.
            """,
        ),
        "_filterer": attr.label(
            default = "//pkg:filter_directory",
            executable = True,
            cfg = "exec",
        ),
    },
)
