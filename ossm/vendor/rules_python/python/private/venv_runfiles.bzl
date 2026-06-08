"""Code for constructing venvs."""

load("@bazel_skylib//lib:paths.bzl", "paths")
load(
    ":common.bzl",
    "is_file",
    "relative_path",
    "runfiles_root_path",
)
load(
    ":py_info.bzl",
    "PyInfo",
    "VenvSymlinkEntry",
    "VenvSymlinkKind",
)
load(":util.bzl", "is_importable_name")

# List of top-level package names that are known to be namespace
# packages, but cannot be detected as such automatically.
_WELL_KNOWN_NAMESPACE_PACKAGES = [
    # nvidia wheels incorrectly use an empty `__init__.py` file, even
    # though multiple distributions install into the directory.
    "nvidia",
]

def create_venv_app_files(ctx, deps, venv_dir_map):
    """Creates the tree of app-specific files for a venv for a binary.

    App specific files are the files that come from dependencies.

    Args:
        ctx: {type}`ctx` current ctx.
        deps: {type}`list[Target]` the targets whose venv information
            to put into the returned venv files.
        venv_dir_map: mapping of VenvSymlinkKind constants to the
            venv path. This tells the directory name of
            platform/configuration-dependent directories. The values are
            paths within the current ctx's venv (e.g. `_foo.venv/bin`).

    Returns:
        {type}`struct` with the following attributes:
        * {type}`list[File]` `venv_files` additional files created for
          the venv.
        * {type}`dict[str, File]` `runfiles_symlinks` map intended for
          the `runfiles.symlinks` argument. A map of main-repo
          relative paths to File.
    """

    # maps venv-relative path to the runfiles path it should point to
    entries = depset(
        transitive = [
            dep[PyInfo].venv_symlinks
            for dep in deps
            if PyInfo in dep
        ],
    ).to_list()

    link_map = build_link_map(ctx, entries)
    venv_files = []
    runfiles_symlinks = {}

    for kind, kind_map in link_map.items():
        base = venv_dir_map[kind]
        for venv_path, link_to in kind_map.items():
            bin_venv_path = paths.join(base, venv_path)
            if is_file(link_to):
                # use paths.join to handle ctx.label.package = ""
                # runfile_prefix should be prepended as we use runfiles.root_symlinks
                runfile_prefix = ctx.label.repo_name or ctx.workspace_name
                symlink_from = paths.join(runfile_prefix, ctx.label.package, bin_venv_path)

                runfiles_symlinks[symlink_from] = link_to
            else:
                venv_link = ctx.actions.declare_symlink(bin_venv_path)
                venv_link_rf_path = runfiles_root_path(ctx, venv_link.short_path)
                rel_path = relative_path(
                    # dirname is necessary because a relative symlink is relative to
                    # the directory the symlink resides within.
                    from_ = paths.dirname(venv_link_rf_path),
                    to = link_to,
                )
                ctx.actions.symlink(output = venv_link, target_path = rel_path)
                venv_files.append(venv_link)

    return struct(
        venv_files = venv_files,
        runfiles_symlinks = runfiles_symlinks,
    )

# Visible for testing
def build_link_map(ctx, entries, return_conflicts = False):
    """Compute the mapping of venv paths to their backing objects.

    Args:
        ctx: {type}`ctx` current ctx.
        entries: {type}`list[VenvSymlinkEntry]` the entries that describe the
            venv-relative
        return_conflicts: {type}`bool`. Only present for testing. If True,
            also return a list of the groups that had overlapping paths and had
            to be resolved and merged.

    Returns:
        {type}`dict[str, dict[str, str|File]]` Mappings of venv paths to their
        backing files. The first key is a `VenvSymlinkKind` value.
        The inner dict keys are venv paths relative to the kind's directory. The
        inner dict values are strings or Files to link to.
    """

    version_by_pkg = {}  # dict[str pkg, str version]
    entries_by_kind = {}  # dict[str kind, list[entry]]

    # Group by path kind and reduce to a single package's version of entries
    for entry in entries:
        entries_by_kind.setdefault(entry.kind, [])
        if not entry.package:
            entries_by_kind[entry.kind].append(entry)
            continue
        if entry.package not in version_by_pkg:
            version_by_pkg[entry.package] = entry.version
            entries_by_kind[entry.kind].append(entry)
            continue
        if entry.version == version_by_pkg[entry.package]:
            entries_by_kind[entry.kind].append(entry)
            continue

        # else: ignore it; not the selected version

    # final paths to keep, grouped by kind
    keep_link_map = {}  # dict[str kind, dict[path, str|File]]
    conflicts = [] if return_conflicts else None
    for kind, entries in entries_by_kind.items():
        # dict[str kind-relative path, str|File link_to]
        keep_kind_link_map = {}

        groups = _group_venv_path_entries(entries)

        for group in groups:
            # If there's just one group, we can symlink to the directory
            if len(group) == 1:
                entry = group[0]
                if entry.link_to_file:
                    keep_kind_link_map[entry.venv_path] = entry.link_to_file
                else:
                    keep_kind_link_map[entry.venv_path] = entry.link_to_path
            else:
                if return_conflicts:
                    conflicts.append(group)

                # Merge a group of overlapping prefixes
                _merge_venv_path_group(ctx, group, keep_kind_link_map)

        keep_link_map[kind] = keep_kind_link_map
    if return_conflicts:
        return keep_link_map, conflicts
    else:
        return keep_link_map

def _group_venv_path_entries(entries):
    """Group entries by VenvSymlinkEntry.venv_path overlap.

    This does an initial grouping by the top-level venv path an entry wants.
    Entries that are underneath another entry are put into the same group.

    Returns:
        {type}`list[list[VenvSymlinkEntry]]` The inner list is the entries under
        a common venv path. The inner list is ordered from shortest to longest
        path.
    """

    # Sort so order is top-down, ensuring grouping by short common prefix
    # Split it into path components so `foo foo-bar foo/bar` sorts as
    # `foo foo/bar foo-bar`
    entries = sorted(entries, key = lambda e: tuple(e.venv_path.split("/")))

    groups = []
    current_group = None
    current_group_prefix = None
    for entry in entries:
        # NOTE: When a file is being directly linked, the anchored prefix can look
        # odd, e.g. "foo/__init__.py/". This is OK; it's just used to prevent
        # incorrect prefix substring matching.
        anchored_prefix = entry.venv_path + "/"
        if (current_group_prefix == None or
            not anchored_prefix.startswith(current_group_prefix)):
            current_group_prefix = anchored_prefix
            current_group = [entry]
            groups.append(current_group)
        else:
            current_group.append(entry)

    return groups

def _merge_venv_path_group(ctx, group, keep_map):
    """Merges a group of overlapping prefixes.

    Args:
        ctx: {type}`ctx` current ctx.
        group: {type}`list[VenvSymlinkEntry]` a group of entries with overlapping
            `venv_path` prefixes, ordered from shortest to longest path.
        keep_map: {type}`dict[str, str|File]` files kept after merging are
            populated into this map.
    """

    # TODO: Compute the minimum number of entries to create. This can't avoid
    # flattening the files depset, but can lower the number of materialized
    # files significantly. Usually overlaps are limited to a small number
    # of directories. Note that, when doing so, shared libraries need to
    # be symlinked directly, not the directory containing them, due to
    # dynamic linker symlink resolution semantics on Linux.
    for entry in group:
        root_venv_path = entry.venv_path
        anchored_link_to_path = entry.link_to_path + "/"
        for file in entry.files.to_list():
            rf_root_path = runfiles_root_path(ctx, file.short_path)

            # It's a file (or directory) being directly linked and
            # must be directly linked.
            if rf_root_path == entry.link_to_path:
                # For lack of a better option, first added wins.
                if entry.venv_path not in keep_map:
                    keep_map[entry.venv_path] = file

                # Skip anything remaining: anything left is either
                # the same path (first set wins), a suffix (violates
                # preconditions and can't link anyways), or not under
                # the prefix (violates preconditions).
                break
            else:
                # Compute the file-specific venv path. i.e. the relative
                # path of the file under entry.venv_path, joined with
                # entry.venv_path
                head, match, rel_venv_path = rf_root_path.partition(anchored_link_to_path)
                if not match or head:
                    # If link_to_path didn't match, then obviously skip.
                    # If head is non-empty, it means link_to_path wasn't
                    # found at the start
                    # This shouldn't occur in practice, but guard against it
                    # just in case
                    continue

                venv_path = paths.join(root_venv_path, rel_venv_path)

                # For lack of a better option, first added wins.
                if venv_path not in keep_map:
                    keep_map[venv_path] = file

def _get_file_venv_path(ctx, f, site_packages_root):
    """Computes a file's venv_path if it's under the site_packages_root.

    Args:
        ctx: The current ctx.
        f: The file to compute the venv_path for.
        site_packages_root: The site packages root path; repo-relative
            path.

    Returns:
        A tuple `(venv_path, rf_root_path)` if the file is under
        `site_packages_root`, otherwise `(None, None)`.
    """
    rf_root_path = runfiles_root_path(ctx, f.short_path)
    _, _, repo_rel_path = rf_root_path.partition("/")
    head, found_sp_root, venv_path = repo_rel_path.partition(site_packages_root)
    if head or not found_sp_root:
        # If head is set, then the path didn't start with site_packages_root
        # if found_sp_root is empty, then it means it wasn't found at all.
        return (None, None)
    return (venv_path, rf_root_path)

def get_venv_symlinks(
        ctx,
        files,
        package,
        version_str,
        site_packages_root,
        namespace_package_files = []):
    """Compute the VenvSymlinkEntry objects for a library.

    Args:
        ctx: {type}`ctx` the current ctx.
        files: {type}`list[File]` the underlying files that are under
            `site_packages_root` and intended to be part of the venv
            contents.
        package: {type}`str` the Python distribution name.
        version_str: {type}`str` the distribution's version.
        site_packages_root: {type}`str` prefix under which files are
            considered to be part of the installed files.
        namespace_package_files: {type}`list[File]` a list of files
            that are pkgutil-style namespace packages and cannot be
            directly linked.

    Returns:
        {type}`list[VenvSymlinkEntry]` the entries that describe how
        to map the files into a venv.
    """
    if site_packages_root.endswith("/"):
        fail("The `site_packages_root` value cannot end in " +
             "slash, got {}".format(site_packages_root))
    if site_packages_root.startswith("/"):
        fail("The `site_packages_root` cannot start with " +
             "slash, got {}".format(site_packages_root))

    # Append slash to prevent incorrect prefix-string matches
    site_packages_root += "/"

    all_files = sorted(files, key = lambda f: f.short_path)

    # dict[str venv-relative dirname, bool is_namespace_package]
    namespace_package_dirs = {
        ns: True
        for ns in _WELL_KNOWN_NAMESPACE_PACKAGES
    }

    # venv paths that cannot be directly linked. Dict acting as set.
    cannot_be_linked_directly = {
        dirname: True
        for dirname in namespace_package_dirs.keys()
    }
    for f in namespace_package_files:
        venv_path, _ = _get_file_venv_path(ctx, f, site_packages_root)
        if venv_path == None:
            continue
        ns_dir = paths.dirname(venv_path)
        namespace_package_dirs[ns_dir] = True
        cannot_be_linked_directly[ns_dir] = True

    # dict[str path, VenvSymlinkEntry]
    # Where path is the venv path (i.e. relative to site_packages_prefix)
    venv_symlinks = {}

    # List of (File, str venv_path) tuples
    files_left_to_link = []

    # We want to minimize the number of files symlinked. Ideally, only the
    # top-level directories are symlinked. Unfortunately, shared libraries
    # complicate matters: if a shared library's directory is linked, then the
    # dynamic linker computes the wrong search path.
    #
    # To fix, we have to directly link shared libraries. This then means that
    # all the parent directories of the shared library can't be linked
    # directly.
    for src in all_files:
        venv_path, rf_root_path = _get_file_venv_path(ctx, src, site_packages_root)
        if venv_path == None:
            continue

        filename = paths.basename(venv_path)
        if _is_linker_loaded_library(filename):
            venv_symlinks[venv_path] = VenvSymlinkEntry(
                kind = VenvSymlinkKind.LIB,
                link_to_path = rf_root_path,
                link_to_file = src,
                package = package,
                version = version_str,
                files = depset([src]),
                venv_path = venv_path,
            )
            parent = paths.dirname(venv_path)
            for _ in range(len(venv_path) + 1):  # Iterate enough times to traverse up
                if not parent:
                    break
                if cannot_be_linked_directly.get(parent, False):
                    # Already seen
                    break
                cannot_be_linked_directly[parent] = True
                parent = paths.dirname(parent)
        else:
            files_left_to_link.append((src, venv_path))

        top_level_dirname, _, tail = venv_path.partition("/")
        if (
            # If it's already not directly linkable, nothing to do
            not cannot_be_linked_directly.get(top_level_dirname, False) and
            # If its already known to be non-implicit namespace, then skip
            namespace_package_dirs.get(top_level_dirname, True) and
            # It must be an importable name to be an implicit namespace package
            is_importable_name(top_level_dirname)
        ):
            namespace_package_dirs.setdefault(top_level_dirname, True)

            # Looking for `__init__.` isn't 100% correct, as it'll match e.g.
            # `__init__.pyi`, but it's close enough.
            if "/" not in tail and tail.startswith("__init__."):
                namespace_package_dirs[top_level_dirname] = False

    # We treat namespace packages as a hint that other distributions may
    # install into the same directory. As such, we avoid linking them directly
    # to avoid conflict merging later.
    for dirname, is_namespace_package in namespace_package_dirs.items():
        if is_namespace_package:
            # If it's already in cannot_be_linked_directly due to pkgutil_namespace_packages
            # then we should not unset it.
            if not cannot_be_linked_directly.get(dirname, False):
                cannot_be_linked_directly[dirname] = True

    # At this point, venv_symlinks has entries for the shared libraries
    # and cannot_be_linked_directly has the directories that cannot be
    # directly linked. Next, we loop over the remaining files and group
    # them into the highest level directory that can be linked.

    # dict[str venv_path, list[File]]
    optimized_groups = {}

    for src, venv_path in files_left_to_link:
        parent = paths.dirname(venv_path)
        if not parent:
            # File in root, must be linked directly
            optimized_groups.setdefault(venv_path, [])
            optimized_groups[venv_path].append(src)
            continue

        if parent in cannot_be_linked_directly:
            # File in a directory that cannot be directly linked,
            # so link the file directly
            optimized_groups.setdefault(venv_path, [])
            optimized_groups[venv_path].append(src)
        else:
            # This path can be grouped. Find the highest-level directory to link.
            venv_path = parent
            next_parent = paths.dirname(parent)
            for _ in range(len(venv_path) + 1):  # Iterate enough times
                if next_parent:
                    if next_parent not in cannot_be_linked_directly:
                        venv_path = next_parent
                        next_parent = paths.dirname(next_parent)
                    else:
                        break
                else:
                    break

            optimized_groups.setdefault(venv_path, [])
            optimized_groups[venv_path].append(src)

    # Finally, for each group, we create the VenvSymlinkEntry objects
    for venv_path, files in optimized_groups.items():
        link_to_path = (
            _get_label_runfiles_repo(ctx, files[0].owner) +
            "/" +
            site_packages_root +
            venv_path
        )
        venv_symlinks[venv_path] = VenvSymlinkEntry(
            kind = VenvSymlinkKind.LIB,
            link_to_path = link_to_path,
            link_to_file = None,
            package = package,
            version = version_str,
            venv_path = venv_path,
            files = depset(files),
        )

    return venv_symlinks.values()

def _is_linker_loaded_library(filename):
    """Tells if a filename is one that `dlopen()` or the runtime linker handles.

    C libraries: *.so (linux), *.so.* (linux), *.dylib (mac), .dll (windows)
    """
    if filename.endswith(".dll"):
        return True
    if filename.endswith((".so", ".dylib")) or ".so." in filename:
        return True
    return False

def _get_label_runfiles_repo(ctx, label):
    repo = label.repo_name
    if repo:
        return repo
    else:
        # For files, empty repo means the main repo
        return ctx.workspace_name
