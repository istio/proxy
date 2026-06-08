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
"""Internal functions for processing pkg_file* instances.

Concepts and terms:

  DestFile: A provider holding the source path, attributes and other
            information about a file that should appear in the package.
            When attributes are empty in DestFile, we let the package
            tool decide their values.

  content map: The map of destination paths to DestFile instances. Note that
               several distinct destinations make share the same source path.
               Attempting to insert a duplicate entry in the content map is
               an error, because it means you are collapsing files together.
               We may want to relax this in the future if their DestFiles
               are equal.

  manifest: The file which represents the content map. This is generated
            by rule implementations and passed to the build_*.py helpers.
"""

load("//pkg:path.bzl", "compute_data_path", "dest_path")
load(
    "//pkg:providers.bzl",
    "PackageDirsInfo",
    "PackageFilegroupInfo",
    "PackageFilesInfo",
    "PackageSymlinkInfo",
)
load(
    "//pkg/private:util.bzl",
    "get_files_to_run_provider",
    "get_repo_mapping_manifest",
)

ENTRY_IS_RAW_LINK = "raw_symlink"  # Entry is a symlink kept as-is
ENTRY_IS_FILE = "file"  # Entry is a file: take content from <src>
ENTRY_IS_LINK = "symlink"  # Entry is a symlink: dest -> <src>
ENTRY_IS_DIR = "dir"  # Entry is an empty dir
ENTRY_IS_TREE = "tree"  # Entry is a tree artifact: take tree from <src>
ENTRY_IS_EMPTY_FILE = "empty-file"  # Entry is a an empty file

# buildifier: disable=name-conventions
_DestFile = provider(
    doc = """Information about each destination in the final package.""",
    fields = {
        "src": "source file",
        "mode": "mode, or empty",
        "user": "user, or empty",
        "group": "group, or empty",
        "link_to": "path to link to. src must not be set",
        "entry_type": "string.  See ENTRY_IS_* values above.",
        "origin": "target which added this",
        "uid": "uid, or empty",
        "gid": "gid, or empty",
    },
)

# buildifier: disable=name-conventions
_MappingContext = provider(
    doc = """Fields passed to process_* methods.""",
    fields = {
        "content_map": "in/out The content_map we are building up",
        "file_deps": "in/out list of file Depsets represented in the map",
        "label": "ctx.label",

        # Behaviors
        "allow_duplicates_with_different_content": "bool: don't fail when you double mapped files",
        "include_runfiles": "bool: include runfiles",
        "workspace_name": "string: name of the main workspace",
        "strip_prefix": "strip_prefix",
        "path_mapper": "function to map destination paths",

        # Defaults
        "default_mode": "Default mode to apply to file without a mode setting",
        "default_user": "Default user name to apply to file without a user",
        "default_group": "Default group name to apply to file without a group",
        "default_uid": "Default numeric uid to apply to file without a uid",
        "default_gid": "Default numeric gid to apply to file without a gid",
    },
)

# buildifier: disable=function-docstring-args
def create_mapping_context_from_ctx(
        ctx,
        label,
        allow_duplicates_with_different_content = None,
        strip_prefix = None,
        include_runfiles = None,
        default_mode = None,
        path_mapper = None):
    """Construct a MappingContext.

    Args: See the provider definition.

    Returns:
        _MappingContext
    """
    if allow_duplicates_with_different_content == None:
        allow_duplicates_with_different_content = ctx.attr.allow_duplicates_with_different_content if hasattr(ctx.attr, "allow_duplicates_with_different_content") else False
    if strip_prefix == None:
        strip_prefix = ctx.attr.strip_prefix if hasattr(ctx.attr, "strip_prefix") else ""
    if include_runfiles == None:
        include_runfiles = ctx.attr.include_runfiles if hasattr(ctx.attr, "include_runfiles") else False
    if default_mode == None:
        default_mode = ctx.attr.mode if hasattr(ctx.attr, "default_mode") else ""

    return _MappingContext(
        content_map = dict(),
        file_deps = list(),
        label = label,
        allow_duplicates_with_different_content = allow_duplicates_with_different_content,
        strip_prefix = strip_prefix,
        include_runfiles = include_runfiles,
        workspace_name = ctx.workspace_name,
        default_mode = default_mode,
        path_mapper = path_mapper or (lambda x: x),
        # TODO(aiuto): allow these to be passed in as needed. But, before doing
        # that, explore defauilt_uid/gid as 0 rather than None
        default_user = "",
        default_group = "",
        default_uid = None,
        default_gid = None,
    )

def _check_dest(content_map, dest, src, origin, allow_duplicates_with_different_content = False):
    old_entry = content_map.get(dest)
    if not old_entry:
        return
    if old_entry.src == src or old_entry.origin == origin:
        return

    # TODO(#385): This is insufficient but good enough for now. We should
    # compare over all the attributes too. That will detect problems where
    # people specify the owner in one place, but another overly broad glob
    # brings in the file with a different owner.
    if old_entry.src.path != src.path:
        msg = "Duplicate output path: <%s>, declared in %s and %s\n  SRC: %s" % (
            dest,
            origin,
            content_map[dest].origin,
            src,
        )
        if allow_duplicates_with_different_content:
            # buildifier: disable=print
            print("WARNING:", msg)
        else:
            # When we default to this behaviour, we should consider telling
            # users the attribute to set to deal with this.
            # For now though, let's not, since they've explicitly opted in.
            fail(msg)

def _merge_attributes(info, mode, user, group, uid, gid):
    if hasattr(info, "attributes"):
        attrs = info.attributes
        mode = attrs.get("mode") or mode
        user = attrs.get("user") or user
        group = attrs.get("group") or group

        new_uid = attrs.get("uid")
        if new_uid != None:
            uid = new_uid
        new_gid = attrs.get("gid")
        if new_gid != None:
            gid = new_gid
    return (mode, user, group, uid, gid)

def _merge_context_attributes(info, mapping_context):
    """Merge defaults from mapping context with those in the source provider.

    Args:
        info: provider from a pkt_* target
        mapping_context: MappingContext with the defaults.
    """
    default_mode = mapping_context.default_mode if hasattr(mapping_context, "default_mode") else ""
    default_user = mapping_context.default_user if hasattr(mapping_context, "default_user") else ""
    default_group = mapping_context.default_group if hasattr(mapping_context, "default_group") else ""
    default_uid = mapping_context.default_uid if hasattr(mapping_context, "default_uid") else ""
    default_gid = mapping_context.default_gid if hasattr(mapping_context, "default_gid") else ""
    return _merge_attributes(info, default_mode, default_user, default_group, default_uid, default_gid)

def _process_pkg_dirs(mapping_context, pkg_dirs_info, origin):
    attrs = _merge_context_attributes(pkg_dirs_info, mapping_context)
    for dir in pkg_dirs_info.dirs:
        dest = dir.strip("/")
        _check_dest(mapping_context.content_map, dest, None, origin, mapping_context.allow_duplicates_with_different_content)
        mapping_context.content_map[dest] = _DestFile(
            src = None,
            entry_type = ENTRY_IS_DIR,
            mode = attrs[0],
            user = attrs[1],
            group = attrs[2],
            uid = attrs[3],
            gid = attrs[4],
            origin = origin,
        )

def _process_pkg_files(mapping_context, pkg_files_info, origin):
    attrs = _merge_context_attributes(pkg_files_info, mapping_context)
    for filename, src in pkg_files_info.dest_src_map.items():
        dest = filename.strip("/")
        _check_dest(mapping_context.content_map, dest, src, origin, mapping_context.allow_duplicates_with_different_content)
        mapping_context.content_map[dest] = _DestFile(
            src = src,
            entry_type = ENTRY_IS_TREE if src.is_directory else ENTRY_IS_FILE,
            mode = attrs[0],
            user = attrs[1],
            group = attrs[2],
            uid = attrs[3],
            gid = attrs[4],
            origin = origin,
        )

def _process_pkg_symlink(mapping_context, pkg_symlink_info, origin):
    dest = pkg_symlink_info.destination.strip("/")
    attrs = _merge_context_attributes(pkg_symlink_info, mapping_context)
    _check_dest(mapping_context.content_map, dest, None, origin, mapping_context.allow_duplicates_with_different_content)
    mapping_context.content_map[dest] = _DestFile(
        src = None,
        entry_type = ENTRY_IS_LINK,
        mode = attrs[0],
        user = attrs[1],
        group = attrs[2],
        uid = attrs[3],
        gid = attrs[4],
        origin = origin,
        link_to = pkg_symlink_info.target,
    )

def _process_pkg_filegroup(mapping_context, pkg_filegroup_info):
    if hasattr(pkg_filegroup_info, "pkg_dirs"):
        for d in pkg_filegroup_info.pkg_dirs:
            _process_pkg_dirs(mapping_context, d[0], d[1])
    if hasattr(pkg_filegroup_info, "pkg_files"):
        for pf in pkg_filegroup_info.pkg_files:
            _process_pkg_files(mapping_context, pf[0], pf[1])
    if hasattr(pkg_filegroup_info, "pkg_symlinks"):
        for psl in pkg_filegroup_info.pkg_symlinks:
            _process_pkg_symlink(mapping_context, psl[0], psl[1])

def process_src(mapping_context, src, origin):
    """Add an entry to the content map.

    Args:
      mapping_context: (r/w) a MappingContext
      src: Source Package*Info object
      origin: The rule instance adding this entry

    Returns:
      True if src was a Package*Info and added to content_map.
    """

    # Gather the files for every srcs entry here, even if it is not from
    # a pkg_* rule.
    if DefaultInfo in src:
        mapping_context.file_deps.append(src[DefaultInfo].files)
    found_info = False
    if PackageFilesInfo in src:
        _process_pkg_files(
            mapping_context,
            src[PackageFilesInfo],
            origin,
        )
        found_info = True
    if PackageFilegroupInfo in src:
        _process_pkg_filegroup(
            mapping_context,
            src[PackageFilegroupInfo],
        )
        found_info = True
    if PackageSymlinkInfo in src:
        _process_pkg_symlink(
            mapping_context,
            src[PackageSymlinkInfo],
            origin,
        )
        found_info = True
    if PackageDirsInfo in src:
        _process_pkg_dirs(
            mapping_context,
            src[PackageDirsInfo],
            origin,
        )
        found_info = True
    return found_info

def add_directory(mapping_context, dir_path, origin, mode = None, user = None, group = None, uid = None, gid = None):
    """Add an empty directory to the content map.

    Args:
      mapping_context: (r/w) a MappingContext
      dir_path: Where to place the file in the package.
      origin: The rule instance adding this entry
      mode: fallback mode to use for Package*Info elements without mode
      user: fallback user to use for Package*Info elements without user
      group: fallback mode to use for Package*Info elements without group
      uid: numeric uid
      gid: numeric gid
    """
    mapping_context.content_map[dir_path.strip("/")] = _DestFile(
        src = None,
        entry_type = ENTRY_IS_DIR,
        origin = origin,
        mode = mode,
        user = user or mapping_context.default_user,
        group = group or mapping_context.default_group,
        uid = uid or mapping_context.default_uid,
        gid = gid or mapping_context.default_gid,
    )

def add_empty_file(mapping_context, dest_path, origin, mode = None, user = None, group = None, uid = None, gid = None):
    """Add a single file to the content map.

    Args:
      mapping_context: (r/w) a MappingContext
      dest_path: Where to place the file in the package.
      origin: The rule instance adding this entry
      mode: fallback mode to use for Package*Info elements without mode
      user: fallback user to use for Package*Info elements without user
      group: fallback mode to use for Package*Info elements without group
      uid: numeric uid
      gid: numeric gid
    """
    dest = dest_path.strip("/")
    _check_dest(mapping_context.content_map, dest, None, origin)
    mapping_context.content_map[dest] = _DestFile(
        src = None,
        entry_type = ENTRY_IS_EMPTY_FILE,
        origin = origin,
        mode = mode,
        user = user or mapping_context.default_user,
        group = group or mapping_context.default_group,
        uid = uid or mapping_context.default_uid,
        gid = gid or mapping_context.default_gid,
    )

def add_label_list(mapping_context, srcs):
    """Helper method to add a list of labels (typically 'srcs') to a content_map.

    Args:
      mapping_context: (r/w) a MappingContext
      srcs: List of source objects
    """

    # Compute the relative path
    data_path = compute_data_path(
        mapping_context.label,
        mapping_context.strip_prefix,
    )
    data_path_without_prefix = compute_data_path(
        mapping_context.label,
        ".",
    )

    for src in srcs:
        if not process_src(
            mapping_context,
            src = src,
            origin = src.label,
        ):
            # Add in the files of srcs which are not pkg_* types
            add_from_default_info(
                mapping_context,
                src,
                data_path,
                data_path_without_prefix,
                mapping_context.include_runfiles,
                mapping_context.workspace_name,
            )

def add_from_default_info(
        mapping_context,
        src,
        data_path,
        data_path_without_prefix,
        include_runfiles,
        workspace_name):
    """Helper method to add the DefaultInfo of a target to a content_map.

    Args:
      mapping_context: (r/w) a MappingContext
      src: A source object.
      data_path: path to package
      data_path_without_prefix: path to the package after prefix stripping
      include_runfiles: Include runfiles
      workspace_name: name of the main workspace
    """
    if not DefaultInfo in src:
        return

    # Auto-detect the executable so we can set its mode.
    the_executable = get_my_executable(src)
    all_files = src[DefaultInfo].files.to_list()
    for f in all_files:
        d_path = mapping_context.path_mapper(
            dest_path(f, data_path, data_path_without_prefix),
        )
        if f.is_directory:
            add_tree_artifact(
                mapping_context.content_map,
                dest_path = d_path,
                src = f,
                origin = src.label,
                mode = mapping_context.default_mode,
                user = mapping_context.default_user,
                group = mapping_context.default_group,
            )
        else:
            fmode = "0755" if f == the_executable else mapping_context.default_mode
            add_single_file(
                mapping_context,
                dest_path = d_path,
                src = f,
                origin = src.label,
                mode = fmode,
                user = mapping_context.default_user,
                group = mapping_context.default_group,
            )

    if include_runfiles:
        runfiles = src[DefaultInfo].default_runfiles
        if runfiles and runfiles.files:
            mapping_context.file_deps.append(runfiles.files)

            # Computing the runfiles root is subtle. It should be based off of
            # the executable, but that is not always obvious. When in doubt,
            # the first file of DefaultInfo.files should be the right target.
            base_file = the_executable or all_files[0]
            base_file_path = dest_path(base_file, data_path, data_path_without_prefix)
            base_path = base_file_path + ".runfiles/" + workspace_name

            for rf in runfiles.files.to_list():
                d_path = mapping_context.path_mapper(base_path + "/" + rf.short_path)
                fmode = "0755" if rf == the_executable else mapping_context.default_mode
                _check_dest(mapping_context.content_map, d_path, rf, src.label, mapping_context.allow_duplicates_with_different_content)
                if hasattr(rf, "is_symlink") and rf.is_symlink:  # File.is_symlink is Bazel 8+
                    entry_type = ENTRY_IS_RAW_LINK
                elif rf.is_directory:
                    entry_type = ENTRY_IS_TREE
                else:
                    entry_type = ENTRY_IS_FILE

                mapping_context.content_map[d_path] = _DestFile(
                    src = rf,
                    entry_type = entry_type,
                    origin = src.label,
                    mode = fmode,
                    user = mapping_context.default_user,
                    group = mapping_context.default_group,
                    uid = mapping_context.default_uid,
                    gid = mapping_context.default_gid,
                )

            # if repo_mapping manifest exists (for e.g. with --enable_bzlmod),
            # create _repo_mapping under runfiles directory
            repo_mapping_manifest = get_repo_mapping_manifest(src)
            if repo_mapping_manifest:
                mapping_context.file_deps.append(depset([repo_mapping_manifest]))

                # TODO: This should really be a symlink into .runfiles/_repo_mapping
                # that also respects remap_paths. For now this is duplicated with the
                # repo_mapping file within the runfiles directory
                d_path = mapping_context.path_mapper(dest_path(
                    repo_mapping_manifest,
                    data_path,
                    data_path_without_prefix,
                ))
                add_single_file(
                    mapping_context,
                    dest_path = d_path,
                    src = repo_mapping_manifest,
                    origin = src.label,
                    mode = mapping_context.default_mode,
                    user = mapping_context.default_user,
                    group = mapping_context.default_group,
                )

                runfiles_repo_mapping_path = mapping_context.path_mapper(
                    base_file_path + ".runfiles/_repo_mapping",
                )
                add_single_file(
                    mapping_context,
                    dest_path = runfiles_repo_mapping_path,
                    src = repo_mapping_manifest,
                    origin = src.label,
                    mode = mapping_context.default_mode,
                    user = mapping_context.default_user,
                    group = mapping_context.default_group,
                )

def get_my_executable(src):
    """If a target represents an executable, return its file handle.

    The roundabout hackery here is because there is no good way to see if
    DefaultInfo was created with an executable in it.
    See: https://github.com/bazelbuild/bazel/issues/14811

    Args:
      src: A label.
    Returns:
      File or None.
    """

    files_to_run_provider = get_files_to_run_provider(src)

    # The docs lead you to believe that you could look at
    # files_to_run.executable, but that is filled out even for source
    # files.
    if getattr(files_to_run_provider, "runfiles_manifest"):
        # DEBUG print("Got an manifest executable", files_to_run_provider.executable)
        return files_to_run_provider.executable
    return None

def add_single_file(mapping_context, dest_path, src, origin, mode = None, user = None, group = None, uid = None, gid = None):
    """Add an single file to the content map.

    Args:
      mapping_context: the MappingContext
      dest_path: Where to place the file in the package.
      src: Source object. Must have len(src[DefaultInfo].files) == 1
      origin: The rule instance adding this entry
      mode: fallback mode to use for Package*Info elements without mode
      user: fallback user to use for Package*Info elements without user
      group: fallback mode to use for Package*Info elements without group
      uid: numeric uid
      gid: numeric gid
    """
    dest = dest_path.strip("/")
    _check_dest(mapping_context.content_map, dest, src, origin, mapping_context.allow_duplicates_with_different_content)

    if hasattr(src, "is_symlink") and src.is_symlink:  # File.is_symlink is Bazel 8+
        entry_type = ENTRY_IS_RAW_LINK
    else:
        entry_type = ENTRY_IS_FILE

    mapping_context.content_map[dest] = _DestFile(
        src = src,
        entry_type = entry_type,
        origin = origin,
        mode = mode,
        user = user or mapping_context.default_user,
        group = group or mapping_context.default_group,
        uid = uid or mapping_context.default_uid,
        gid = gid or mapping_context.default_gid,
    )

def add_symlink(mapping_context, dest_path, src, origin):
    """Add a symlink to the content map.

    TODO(aiuto): This is a vestige left from the pkg_tar use.  We could
    converge code by having pkg_tar be a macro that expands symlinks to
    pkg_symlink targets and srcs them in.

    Args:
      mapping_context: the MappingContext
      dest_path: Where to place the file in the package.
      src: Path to link to.
      origin: The rule instance adding this entry
    """
    dest = dest_path.strip("/")
    _check_dest(mapping_context.content_map, dest, None, origin)
    mapping_context.content_map[dest] = _DestFile(
        src = None,
        link_to = src,
        entry_type = ENTRY_IS_LINK,
        origin = origin,
        mode = mapping_context.default_mode,
        user = mapping_context.default_user,
        group = mapping_context.default_group,
        uid = mapping_context.default_uid,
        gid = mapping_context.default_gid,
    )

def add_tree_artifact(content_map, dest_path, src, origin, mode = None, user = None, group = None, uid = None, gid = None):
    """Add an tree artifact (directory output) to the content map.

    Args:
      content_map: The content map
      dest_path: Where to place the file in the package.
      src: Source object. Must have len(src[DefaultInfo].files) == 1
      origin: The rule instance adding this entry
      mode: fallback mode to use for Package*Info elements without mode
      user: User name for the entry (probably unused)
      group: group name for the entry (probably unused)
      uid: User id for the entry (probably unused)
      gid: Group id for the entry (probably unused)
    """
    content_map[dest_path] = _DestFile(
        src = src,
        origin = origin,
        entry_type = ENTRY_IS_TREE,
        mode = mode,
        user = user,
        group = group,
        uid = uid,
        gid = gid,
    )

def write_manifest(ctx, manifest_file, content_map, use_short_path = False, pretty_print = False):
    """Write a content map to a manifest file.

    The format of this file is currently undocumented, as it is a private
    contract between the rule implementation and the package writers.  It will
    become a published interface in a future release.

    For reproducibility, the manifest file must be ordered consistently.

    Args:
      ctx: rule context
      manifest_file: File object used as the output destination
      content_map: content_map (see concepts at top of file)
      use_short_path: write out the manifest file destinations in terms of "short" paths, suitable for `bazel run`.
      pretty_print: indent the output nicely. Takes more space so it is off by default.
    """
    ctx.actions.write(
        manifest_file,
        "[\n" + ",\n".join(
            [
                _encode_manifest_entry(dst, content_map[dst], use_short_path, pretty_print)
                for dst in sorted(content_map.keys())
            ],
        ) + "\n]\n",
    )

def _encode_manifest_entry(dest, df, use_short_path, pretty_print = False):
    entry_type = df.entry_type if hasattr(df, "entry_type") else ENTRY_IS_FILE
    if df.src:
        src = df.src.short_path if use_short_path else df.src.path
        # entry_type is left as-is

    elif hasattr(df, "link_to"):
        src = df.link_to
        entry_type = ENTRY_IS_LINK
    else:
        src = None

    # Bazel 6 has a new flag "--incompatible_unambiguous_label_stringification"
    # (https://github.com/bazelbuild/bazel/issues/15916) that causes labels in
    # the repository in which Bazel was run to be stringified with a preceding
    # "@".  In older versions, this flag did not exist.
    #
    # Since this causes all sorts of chaos with our tests, be consistent across
    # all Bazel versions.
    origin_str = str(df.origin)
    if not origin_str.startswith("@"):
        origin_str = "@" + origin_str

    data = {
        "type": entry_type,
        "src": src,
        "dest": dest.strip("/"),
        "mode": df.mode or "",
        "user": df.user or None,
        "group": df.group or None,
        "uid": df.uid,
        "gid": df.gid,
        "origin": origin_str,
    }

    if pretty_print:
        return json.encode_indent(data)
    else:
        return json.encode(data)
