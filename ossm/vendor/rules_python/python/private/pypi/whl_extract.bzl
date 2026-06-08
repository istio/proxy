"""A simple whl extractor."""

load("@rules_python_internal//:rules_python_config.bzl", rp_config = "config")
load("//python/private:repo_utils.bzl", "repo_utils")
load(":whl_metadata.bzl", "find_whl_metadata")

def whl_extract(rctx, *, whl_path, logger):
    """Extract whls in Starlark.

    Args:
        rctx: the repository ctx.
        whl_path: the whl path to extract.
        logger: The logger to use
    """
    install_dir_path = rctx.path("site-packages")
    repo_utils.extract(
        rctx,
        archive = whl_path,
        output = install_dir_path,
        supports_whl_extraction = rp_config.supports_whl_extraction,
    )

    # Fix permissions on extracted files. Some wheels have files without read permissions set,
    # which causes errors when trying to read them later.
    os_name = repo_utils.get_platforms_os_name(rctx)
    if os_name != "windows":
        # On Unix-like systems, recursively add read permissions to all files
        # and ensure directories are traversable (need execute permission)
        result = repo_utils.execute_unchecked(
            rctx,
            op = "Fixing wheel permissions {}".format(whl_path),
            arguments = ["chmod", "-R", "a+rX", str(install_dir_path)],
            logger = logger,
        )
        if result.return_code != 0:
            # It's possible chmod is not available or the filesystem doesn't support it.
            # This is fine, we just want to try to fix permissions if possible.
            logger.warn(lambda: "Failed to fix file permissions: {}".format(result.stderr))
    metadata_file = find_whl_metadata(
        install_dir = install_dir_path,
        logger = logger,
    )

    # Get the <prefix>.dist_info dir name
    dist_info_dir = metadata_file.dirname
    rctx.file(
        dist_info_dir.get_child("INSTALLER"),
        "https://github.com/bazel-contrib/rules_python#pipstar",
    )

    # Get the <prefix>.dist_info dir name
    data_dir = dist_info_dir.dirname.get_child(dist_info_dir.basename[:-len(".dist-info")] + ".data")
    if data_dir.exists:
        for prefix, dest_prefix in {
            # https://docs.python.org/3/library/sysconfig.html#posix-prefix
            # We are taking this from the legacy whl installer config
            "data": "data",
            "headers": "include",
            # In theory there may be directory collisions here, so it would be best to
            # merge the paths here. We are doing for quite a few levels deep. What is
            # more, this code has to be reasonably efficient because some packages like
            # to not put everything to the top level, but to indicate explicitly if
            # something is in `platlib` or `purelib` (e.g. libclang wheel).
            "platlib": "site-packages",
            "purelib": "site-packages",
            "scripts": "bin",
        }.items():
            src = data_dir.get_child(prefix)
            if not src.exists:
                # The prefix does not exist in the wheel, we can continue
                continue

            for (src, dest) in merge_trees(src, rctx.path(dest_prefix)):
                logger.debug(lambda: "Renaming: {} -> {}".format(src, dest))
                rctx.rename(src, dest)

            # TODO @aignas 2025-12-16: when moving scripts to `bin`, rewrite the #!python
            # shebang to be something else, for inspiration look at the hermetic
            # toolchain wrappers

        # Ensure that there is no data dir left
        rctx.delete(data_dir)

def merge_trees(src, dest):
    """Merge src into the destination path.

    This will attempt to merge-move src files to the destination directory if there are
    existing files. Fails at directory depth is 10000 or if there are collisions.

    Args:
        src: {type}`path` a src path to rename.
        dest: {type}`path` a dest path to rename to.

    Returns:
        A list of tuples for src and destination paths.
    """
    ret = []
    remaining = [(src, dest)]
    collisions = []
    for _ in range(10000):
        if collisions or not remaining:
            break

        tmp = []
        for (s, d) in remaining:
            if not d.exists:
                ret.append((s, d))
                continue

            if not s.is_dir or not d.is_dir:
                collisions.append(s)
                continue

            for file_or_dir in s.readdir():
                tmp.append((file_or_dir, d.get_child(file_or_dir.basename)))

        remaining = tmp

    if remaining:
        fail("Exceeded maximum directory depth of 10000 during tree merge.")

    if collisions:
        fail("Detected collisions between {} and {}: {}".format(src, dest, collisions))

    return ret
