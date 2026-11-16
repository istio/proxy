"""Compressed tarballs of `pkg_files`, written by bsdtar and a parallel compressor.

`pkg_tar` writes tarballs in Python (~20MB/s), and compresses with gzip in Python too (or pipes
to a compressor): for large tarballs (eg release binaries with debug info, several GB) that
dominates the build. `compressed_tar` takes the same `pkg_files` (their paths and attributes),
writes an mtree of them, and streams bsdtar (the hermetic tar toolchain) into a hermetic
compressor using all the cores: pigz (`//pigz`) for gzip, or zstd.

```starlark
load("@envoy_toolshed//tarball:compressed_tar.bzl", "compressed_tar")

compressed_tar(
    name = "layer",
    srcs = [":files"],  # pkg_files
    out = "layer.tar.gz",
    compression = "gzip",
)
```

The tarball is reproducible: entries are sorted, owned by root (uid/gid 0, named root) unless
the `pkg_files` set an owner, and have `pkg_tar`'s "portable" mtime (2000-01-01); parent directories are added (root,
0755), and files have their `pkg_files` mode (0644 unless set; 0555 for one without a mode, as
`pkg_tar` does for plain files). Gzip is written without a name or
time in its header (`pigz -n`).
"""

load("@rules_pkg//pkg:providers.bzl", "PackageFilesInfo")

_TAR_TOOLCHAIN_TYPE = "@aspect_bazel_lib//lib:tar_toolchain_type"

# `pkg_tar`'s "portable" mtime (2000-01-01), and its mode for files without one.
_MTIME = 946684800
_DEFAULT_MODE = "0555"
_DIR_MODE = "0755"

# Compressors (with their arguments for stdin to stdout, and the range of their levels).
COMPRESSORS = {
    "gzip": struct(
        args = ["-n"],
        compressor = Label("//pigz"),
        levels = (1, 9),
    ),
    "zstd": struct(
        args = [
            "-T0",
            "-q",
            "-c",
        ],
        compressor = Label("@zstd//:zstd_cli"),
        levels = (1, 19),
    ),
}

def _check_path(path):
    if not path or path.startswith("/") or ".." in path.split("/"):
        fail("compressed_tar: invalid path %r" % path)
    for char in [" ", "\t", "\n", "\\", "#", "="]:
        if char in path:
            fail("compressed_tar: unsupported character %r in path %r" % (char, path))
    return path

def _owner(attributes):
    """The mtree ownership keywords for `pkg_attributes` (root by default, also by name)."""
    keywords = []
    for id_keyword, id_key, name_keyword, name_key in [
        ("uid", "uid", "uname", "user"),
        ("gid", "gid", "gname", "group"),
    ]:
        id = attributes.get(id_key, 0)
        name = attributes.get(name_key) or ("root" if str(id) == "0" else None)
        keywords.append("%s=%s" % (id_keyword, id))
        if name:
            keywords.append("%s=%s" % (name_keyword, name))
    return " ".join(keywords)

def render_mtree(files):
    """Render the mtree for a tarball of `files`.

    Args:
        files: Mapping of a path in the tarball to a `(path of the file, pkg_attributes dict)`.

    Returns:
        The mtree, with the parent directories, sorted.
    """
    dirs = {}
    for path in files:
        parts = _check_path(path).split("/")[:-1]
        for i in range(len(parts)):
            dirs["/".join(parts[:i + 1])] = True

    # Every name is `./`-prefixed: in an mtree, a name without a `/` is relative, and a relative
    # directory changes the current directory for the lines that follow. bsdtar strips the
    # prefix again (`-s`), so the tarball's names are as `pkg_tar`'s (eg `envoy`, not `./envoy`).
    for path in dirs:
        if path in files:
            fail("compressed_tar: %s is both a file and a directory" % path)
    lines = ["#mtree"]
    for path in sorted(list(dirs) + list(files)):
        if path in dirs:
            lines.append("./%s type=dir %s mode=%s time=%d" % (path, _owner({}), _DIR_MODE, _MTIME))
            continue
        content, attributes = files[path]
        lines.append("./%s type=file %s mode=%s time=%d content=%s" % (
            path,
            _owner(attributes),
            attributes.get("mode", _DEFAULT_MODE),
            _MTIME,
            _check_path(content),
        ))
    return "\n".join(lines) + "\n"

def _compressed_tar_impl(ctx):
    files = {}
    inputs = []
    for src in ctx.attr.srcs:
        info = src[PackageFilesInfo]
        for path, file in info.dest_src_map.items():
            if file.is_directory:
                fail("compressed_tar: %s is a directory (%s): only files are supported" % (path, src.label))
            if path in files:
                fail("compressed_tar: %s is in the tarball twice (%s)" % (path, src.label))
            files[path] = (file.path, info.attributes)
            inputs.append(file)

    mtree = ctx.actions.declare_file(ctx.label.name + ".mtree")
    ctx.actions.write(mtree, render_mtree(files))

    tar = ctx.toolchains[_TAR_TOOLCHAIN_TYPE]
    args = ctx.actions.args()
    args.add(tar.tarinfo.binary)
    args.add(mtree)
    args.add(ctx.executable.compressor)
    args.add(ctx.outputs.out)
    args.add_all(ctx.attr.compressor_args)
    ctx.actions.run_shell(
        inputs = depset([mtree] + inputs, transitive = [tar.default.files]),
        tools = [ctx.attr.compressor[DefaultInfo].files_to_run],
        outputs = [ctx.outputs.out],
        arguments = [args],
        env = tar.tarinfo.default_env,
        command = """
set -o pipefail
TAR="$1" MTREE="$2" COMPRESSOR="$3" OUT="$4"
shift 4
"$TAR" --create --file - -s ',^\\./,,' "@$MTREE" | "$COMPRESSOR" "$@" > "$OUT"
""",
        mnemonic = "CompressedTar",
        progress_message = "Writing %{output}",
        toolchain = _TAR_TOOLCHAIN_TYPE,
    )
    return [DefaultInfo(files = depset([ctx.outputs.out]))]

compressed_tar_rule = rule(
    implementation = _compressed_tar_impl,
    doc = "A tarball of `pkg_files`, written by bsdtar into a compressor (see `compressed_tar`).",
    attrs = {
        "compressor": attr.label(
            cfg = "exec",
            doc = "Executable compressing stdin to stdout.",
            executable = True,
            mandatory = True,
        ),
        "compressor_args": attr.string_list(
            doc = "Arguments for the compressor.",
        ),
        "out": attr.output(
            doc = "The compressed tarball.",
            mandatory = True,
        ),
        "srcs": attr.label_list(
            doc = "`pkg_files` to include.",
            mandatory = True,
            providers = [PackageFilesInfo],
        ),
    },
    toolchains = [_TAR_TOOLCHAIN_TYPE],
)

def compressed_tar(name, srcs, out, compression, level = None, **kwargs):
    """A compressed tarball of `pkg_files`, written by bsdtar and a parallel compressor.

    Args:
        name: Target name.
        srcs: `pkg_files` to include.
        out: The compressed tarball, eg `<name>.tar.gz`.
        compression: `gzip` (pigz, `//pigz`) or `zstd`, both using all the cores.
        level: Compression level (gzip 1-9, zstd 1-19), or None for the compressor's default
            (gzip 6, zstd 3).
        **kwargs: Common attributes, eg `tags`, `visibility`.
    """
    if compression not in COMPRESSORS:
        fail("compressed_tar: `compression` must be one of %s, got %r" % (sorted(COMPRESSORS), compression))
    compressor = COMPRESSORS[compression]
    args = list(compressor.args)
    if level != None:
        low, high = compressor.levels
        if type(level) != type(0) or level < low or level > high:
            fail("compressed_tar: %s `level` must be %d-%d, got %r" % (compression, low, high, level))
        args.append("-%d" % level)
    compressed_tar_rule(
        name = name,
        srcs = srcs,
        out = out,
        compressor = compressor.compressor,
        compressor_args = args,
        **kwargs
    )
