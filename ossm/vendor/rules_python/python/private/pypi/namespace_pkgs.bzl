"""Utilities to get where we should write namespace pkg paths."""

load("@bazel_skylib//rules:copy_file.bzl", "copy_file")

_ext = struct(
    py = ".py",
    pyd = ".pyd",
    so = ".so",
    pyc = ".pyc",
)

_TEMPLATE = Label("//python/private/pypi:namespace_pkg_tmpl.py")

def _add_all(dirname, dirs):
    dir_path = "."
    for dir_name in dirname.split("/"):
        dir_path = "{}/{}".format(dir_path, dir_name)
        dirs[dir_path[2:]] = None

def get_files(*, srcs, ignored_dirnames = [], root = None):
    """Get the list of filenames to write the namespace pkg files.

    Args:
        srcs: {type}`src` a list of files to be passed to {bzl:obj}`py_library`
            as `srcs` and `data`. This is usually a result of a {obj}`glob`.
        ignored_dirnames: {type}`str` a list of patterns to ignore.
        root: {type}`str` the prefix to use as the root.

    Returns:
        {type}`src` a list of paths to write the namespace pkg `__init__.py` file.
    """
    dirs = {}
    ignored = {i: None for i in ignored_dirnames}

    if root:
        _add_all(root, ignored)

    for file in srcs:
        dirname, _, filename = file.rpartition("/")

        if filename == "__init__.py":
            ignored[dirname] = None
            dirname, _, _ = dirname.rpartition("/")
        elif filename.endswith(_ext.py):
            pass
        elif filename.endswith(_ext.pyc):
            pass
        elif filename.endswith(_ext.pyd):
            pass
        elif filename.endswith(_ext.so):
            pass
        else:
            continue

        if dirname in dirs or not dirname:
            continue

        _add_all(dirname, dirs)

    return sorted([d for d in dirs if d not in ignored])

def create_inits(*, srcs, ignored_dirnames = [], root = None, copy_file = copy_file, **kwargs):
    """Create init files and return the list to be included `py_library` srcs.

    Args:
        srcs: {type}`src` a list of files to be passed to {bzl:obj}`py_library`
            as `srcs` and `data`. This is usually a result of a {obj}`glob`.
        ignored_dirnames: {type}`str` a list of patterns to ignore.
        root: {type}`str` the prefix to use as the root.
        copy_file: the `copy_file` rule to copy files in build context.
        **kwargs: passed to {obj}`copy_file`.

    Returns:
        {type}`list[str]` to be included as part of `py_library`.
    """
    ret = []
    for i, out in enumerate(get_files(srcs = srcs, ignored_dirnames = ignored_dirnames, root = root)):
        src = "{}/__init__.py".format(out)
        ret.append(src)

        copy_file(
            # For the target name, use a number instead of trying to convert an output
            # path into a valid label.
            name = "_cp_{}_namespace".format(i),
            src = _TEMPLATE,
            out = src,
            **kwargs
        )

    return ret
