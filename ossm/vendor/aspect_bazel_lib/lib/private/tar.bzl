"Implementation of tar rule"

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("//lib:paths.bzl", "to_repository_relative_path")

TAR_TOOLCHAIN_TYPE = "@aspect_bazel_lib//lib:tar_toolchain_type"

# https://www.gnu.org/software/tar/manual/html_section/Compression.html
_ACCEPTED_EXTENSIONS = [
    ".tar",  # uncompressed,
    ".gz",  # gzip
    ".tgz",  # gzip
    ".taz",  # gzip
    ".Z",  # compress
    ".taZ",  # compress
    ".bz2",  # bzip2
    ".tz2",  # bzip2
    ".tbz2",  # bzip2
    ".tbz",  # bzip2
    ".lz",  # lzip
    ".lzma",  # lzma
    ".tlz",  # lzma
    ".lzo",  # lzop
    ".xz",  # xz
    ".zst",  # zstd
    ".tzst",  # zstd
]

_COMPRESSION_TO_EXTENSION = {
    "bzip2": ".tar.bz2",
    "compress": ".tar.Z",
    "gzip": ".tar.gz",
    "lrzip": ".tar.lrz",
    "lz4": ".tar.lz4",
    "lzma": ".tar.lzma",
    "lzop": ".tar.lzo",
    "xz": ".tar.xz",
    "zstd": ".tar.zst",
}

# https://www.gnu.org/software/tar/manual/html_section/Compression.html
_ACCEPTED_COMPRESSION_TYPES = _COMPRESSION_TO_EXTENSION.keys()

_tar_attrs = {
    "args": attr.string_list(
        doc = "Additional flags permitted by BSD tar; see the man page.",
    ),
    "srcs": attr.label_list(
        doc = """\
        Files, directories, or other targets whose default outputs are placed into the tar.

        If any of the srcs are binaries with runfiles, those are copied into the resulting tar as well.
        """,
        allow_files = True,
    ),
    "mode": attr.string(
        doc = """A mode indicator from the following list, copied from the tar manpage:

       - create: Create a new archive containing the specified items.

       Other modes may be added in the future.""",
        #    - append: Like `create`, but new entries are appended to the archive.
        #         Note that this only works on uncompressed archives stored in regular files.
        #         The -f option is required.
        #    - list: List  archive contents to stdout.
        #    - update: Like `append`, but new entries are added only if they have a
        #         modification date newer than the corresponding entry in the archive.
        #        Note that this only works on uncompressed archives stored in
        #        regular files. The -f option	is required.
        #    - extract: Extract to disk from the archive. If a file with the same name
        #        appears more than once in the archive, each copy	 will  be  extracted,
        #        with  later  copies  overwriting  (replacing) earlier copies.
        values = ["create"],  # TODO: support other modes: ["append", "list", "update", "extract"]
        default = "create",
    ),
    "mtree": attr.label(
        doc = "An mtree specification file",
        allow_single_file = True,
        # Mandatory since it's the only way to set constant timestamps
        mandatory = True,
    ),
    "out": attr.output(
        doc = "Resulting tar file to write. If absent, `[name].tar` is written.",
    ),
    "compress": attr.string(
        doc = "Compress the archive file with a supported algorithm.",
        values = _ACCEPTED_COMPRESSION_TYPES,
    ),
    "compute_unused_inputs": attr.int(
        doc = """
Whether to discover and prune input files that will not contribute to the archive.

Unused inputs are discovered by comparing the set of input files in `srcs` to the set
of files referenced by `mtree`. Files not used for content by the mtree specification
will not be read by the `tar` tool when creating the archive and can be pruned from the
input set using the `unused_inputs_list`
[mechanism](https://bazel.build/contribute/codebase#input-discovery).

Benefits: pruning unused input files can reduce the amount of work the build system must
perform. Pruned files are not included in the action cache key; changes to them do not
invalidate the cache entry, which can lead to higher cache hit rates. Actions do not need
to block on the availability of pruned inputs, which can increase the available
parallelism of builds. Pruned files do not need to be transferred to remote-execution
workers, which can reduce network costs.

Risks: pruning an actually-used input file can lead to unexpected, incorrect results. The
comparison performed between `srcs` and `mtree` is currently inexact and may fail to
handle handwritten or externally-derived mtree specifications. However, it is safe to use
this feature when the lines found in `mtree` are derived from one or more `mtree_spec`
rules, filtered and/or merged on whole-line basis only.

Possible values:

    - `compute_unused_inputs = 1`: Always perform unused input discovery and pruning.
    - `compute_unused_inputs = 0`: Never discover or prune unused inputs.
    - `compute_unused_inputs = -1`: Discovery and pruning of unused inputs is controlled by the
        --[no]@aspect_bazel_lib//lib:tar_compute_unused_inputs flag.
        """,
        default = -1,
        values = [-1, 0, 1],
    ),
    "_compute_unused_inputs_flag": attr.label(default = Label("//lib:tar_compute_unused_inputs")),
}

_mtree_attrs = {
    "srcs": attr.label_list(doc = "Files that are placed into the tar", allow_files = True),
    "out": attr.output(doc = "Resulting specification file to write"),
}
_mutate_mtree_attrs = {
    "mtree": attr.label(
        allow_single_file = True,
        doc = "Specifies the path to the mtree file, which describes the directory structure and metadata for the tar file. Must be a single file.",
    ),
    "awk_script": attr.label(
        allow_single_file = True,
        default = "@aspect_bazel_lib//lib/private:modify_mtree.awk",
        doc = "Path to an AWK script used to modify the mtree file. By default, it uses the modify_mtree.awk script.",
    ),
    "srcs": attr.label_list(
        allow_files = True,
        doc = "Files, directories, or other targets whose default outputs will used to create symlinks",
    ),
    "preserve_symlinks": attr.bool(
        default = False,
        doc = "If True, symbolic links in the source files are preserved in the tar file. If False, the links are resolved to their actual targets.",
    ),
    "strip_prefix": attr.string(
        doc = "A prefix to strip from the paths of files and directories when they are added to the tar file.",
    ),
    "package_dir": attr.string(
        doc = "Specifies a base directory within the tar file where all files will be placed. Sets the root directory for the tar contents.",
    ),
    "mtime": attr.string(
        doc = "Specifies the modification time (mtime) to be applied to all files in the tar file. Used for deterministic builds.",
    ),
    "owner": attr.string(
        doc = "Specifies the numeric user ID (UID) for the owner of the files in the tar archive.",
    ),
    "ownername": attr.string(
        doc = "Specifies the name of the owner of the files in the tar archive. Used alongside 'owner'.",
    ),
    "out": attr.output(
        doc = "The output of the mutation, a new mtree file.",
    ),
}

def _add_compression_args(compress, args):
    if compress == "bzip2":
        args.add("--bzip2")
    if compress == "compress":
        args.add("--compress")
    if compress == "gzip":
        args.add("--gzip")
        args.add("--options=gzip:!timestamp")  # See https://datatracker.ietf.org/doc/html/rfc1952#page-5 why this option
    if compress == "lrzip":
        args.add("--lrzip")
    if compress == "lzma":
        args.add("--lzma")
    if compress == "lz4":
        args.add("--lz4")
    if compress == "lzop":
        args.add("--lzop")
    if compress == "xz":
        args.add("--xz")
    if compress == "zstd":
        args.add("--zstd")

def _calculate_runfiles_dir(default_info):
    manifest = default_info.files_to_run.runfiles_manifest

    # Newer versions of Bazel put the manifest besides the runfiles with the suffix .runfiles_manifest.
    # For example, the runfiles directory is named my_binary.runfiles then the manifest is beside the
    # runfiles directory and named my_binary.runfiles_manifest
    # Older versions of Bazel put the manifest file named MANIFEST in the runfiles directory
    # See similar logic:
    # https://github.com/aspect-build/rules_js/blob/c50bd3f797c501fb229cf9ab58e0e4fc11464a2f/js/private/bash.bzl#L63
    if manifest.short_path.endswith("_manifest") or manifest.short_path.endswith("/MANIFEST"):
        # Trim last 9 characters, as that's the length in both cases
        return manifest.short_path[:-9]
    fail("manifest path {} seems malformed".format(manifest.short_path))

def _is_unused_inputs_enabled(attr):
    """Determine whether or not to compute unused inputs.

    Args:
        attr: `tar` rule ctx.attr struct. Must provide `_tar_attrs`.

    Returns: bool. Whether the unused_inputs_list file should be computed.
    """
    if attr.compute_unused_inputs == 1:
        return True
    if attr.compute_unused_inputs == 0:
        return False
    if attr.compute_unused_inputs == -1:
        return attr._compute_unused_inputs_flag[BuildSettingInfo].value

    fail("Unexpected `compute_unused_inputs` value: {}".format(attr.compute_unused_inputs))

def _is_unprunable(file):
    # Some input files cannot be pruned because their name will be misinterpreted by Bazel when reading the unused_inputs_list.
    #   * Filenames containing newlines will be mangled in the line-oriented format of the file.
    #   * Filenames with leading or trailing whitespace will be mangled by the call to String.trim().
    # ref https://github.com/bazelbuild/bazel/blob/678b01a512c0b28c87067cdf5a4e0224a82716c0/src/main/java/com/google/devtools/build/lib/analysis/actions/StarlarkAction.java#L357
    p = file.path
    return p[0].isspace() or p[-1].isspace() or "\n" in p or "\r" in p

def _fmt_pruanble_inputs_line(file):
    if _is_unprunable(file):
        return None

    # The tar.prunable_inputs.txt file has a two columns:
    #   1. vis-encoded paths of the files, used in comparison
    #   2. un-vis-encoded paths of the files, used for reporting back to Bazel after filtering
    path = file.path
    return _vis_encode(path) + " " + path

def _fmt_keep_inputs_line(file):
    # The tar.keep_inputs.txt file has a single column of vis-encoded paths of the files to keep.
    return _vis_encode(file.path)

def _configured_unused_inputs_file(ctx, srcs, keep):
    """
    Compute the unused_inputs_list, if configured.

    Args:
        ctx: `tar` rule context. Must provide `_tar_attrs` and a `coreutils_toolchain_type` toolchain.
        srcs: sequence or depset. The set of all input sources being provided to the `tar` rule.
        keep: sequence or depset. A hardcoded set of sources to consider "used" regardless of whether or not they appear in the mtree.

    Returns: file or None. List of inputs unused by the `Tar` action.
    """
    if not _is_unused_inputs_enabled(ctx.attr):
        return None

    coreutils = ctx.toolchains["@aspect_bazel_lib//lib:coreutils_toolchain_type"].coreutils_info.bin

    prunable_inputs = ctx.actions.declare_file(ctx.attr.name + ".prunable_inputs.txt")
    keep_inputs = ctx.actions.declare_file(ctx.attr.name + ".keep_inputs.txt")
    unused_inputs = ctx.actions.declare_file(ctx.attr.name + ".unused_inputs.txt")

    ctx.actions.write(
        output = prunable_inputs,
        content = ctx.actions.args()
            .set_param_file_format("multiline")
            .add_all(
            srcs,
            map_each = _fmt_pruanble_inputs_line,
        ),
    )
    ctx.actions.write(
        output = keep_inputs,
        content = ctx.actions.args()
            .set_param_file_format("multiline")
            .add_all(
            keep,
            map_each = _fmt_keep_inputs_line,
        ),
    )

    # Unused inputs are inputs that:
    #   * are in the set of PRUNABLE_INPUTS
    #   * are not found in any content= or contents= keyword in the MTREE
    #   * are not in the hardcoded KEEP_INPUTS set
    #
    # Comparison and filtering of PRUNABLE_INPUTS is performed in the vis-encoded representation, stored in field 1,
    # before being written out in the un-vis-encoded form Bazel understands, from field 2.
    #
    # Note: bsdtar (libarchive) accepts both content= and contents= to identify source file:
    # ref https://github.com/libarchive/libarchive/blob/a90e9d84ec147be2ef6a720955f3b315cb54bca3/libarchive/archive_read_support_format_mtree.c#L1640
    #
    # TODO: Make comparison exact by converting all inputs to a canonical vis-encoded form before comparing.
    #       See also: https://github.com/bazel-contrib/bazel-lib/issues/794
    ctx.actions.run_shell(
        outputs = [unused_inputs],
        inputs = [prunable_inputs, keep_inputs, ctx.file.mtree],
        tools = [coreutils],
        command = '''
            "$COREUTILS" join -v 1                                                            \\
                <("$COREUTILS" sort -u "$PRUNABLE_INPUTS")                                    \\
                <("$COREUTILS" sort -u                                                        \\
                    <(grep -o '\\bcontents\\?=\\S*' "$MTREE" | "$COREUTILS" cut -d'=' -f 2-)  \\
                    "$KEEP_INPUTS"                                                            \\
                )                                                                             \\
                | "$COREUTILS" cut -d' ' -f 2-                                                \\
                > "$UNUSED_INPUTS"
        ''',
        env = {
            "COREUTILS": coreutils.path,
            "PRUNABLE_INPUTS": prunable_inputs.path,
            "KEEP_INPUTS": keep_inputs.path,
            "MTREE": ctx.file.mtree.path,
            "UNUSED_INPUTS": unused_inputs.path,
        },
        mnemonic = "UnusedTarInputs",
        toolchain = "@aspect_bazel_lib//lib:coreutils_toolchain_type",
    )

    return unused_inputs

# TODO(3.0): Access field directly after minimum bazel_compatibility advanced to or beyond v7.0.0.
def _repo_mapping_manifest(files_to_run):
    return getattr(files_to_run, "repo_mapping_manifest", None)

def _tar_impl(ctx):
    bsdtar = ctx.toolchains[TAR_TOOLCHAIN_TYPE]
    inputs = ctx.files.srcs[:]
    args = ctx.actions.args()

    # Set mode
    args.add(ctx.attr.mode, format = "--%s")

    # User-provided args first
    args.add_all(ctx.attr.args)

    # Compression args
    _add_compression_args(ctx.attr.compress, args)

    ext = _COMPRESSION_TO_EXTENSION[ctx.attr.compress] if ctx.attr.compress else ".tar"

    out = ctx.outputs.out or ctx.actions.declare_file(ctx.attr.name + ext)
    args.add("--file", out)

    args.add(ctx.file.mtree, format = "@%s")
    inputs.append(ctx.file.mtree)

    repo_mappings = [
        _repo_mapping_manifest(src[DefaultInfo].files_to_run)
        for src in ctx.attr.srcs
    ]
    repo_mappings = [m for m in repo_mappings if m != None]
    inputs.extend(repo_mappings)

    srcs_runfiles = [
        src[DefaultInfo].default_runfiles.files
        for src in ctx.attr.srcs
    ]

    unused_inputs_file = _configured_unused_inputs_file(
        ctx,
        srcs = depset(direct = ctx.files.srcs + repo_mappings, transitive = srcs_runfiles),
        keep = depset(direct = [ctx.file.mtree, bsdtar.tarinfo.binary], transitive = [bsdtar.default.files]),
    )
    if unused_inputs_file:
        inputs.append(unused_inputs_file)

    ctx.actions.run(
        executable = bsdtar.tarinfo.binary,
        inputs = depset(direct = inputs, transitive = [bsdtar.default.files] + srcs_runfiles),
        outputs = [out],
        arguments = [args],
        mnemonic = "Tar",
        unused_inputs_list = unused_inputs_file,
        toolchain = "@aspect_bazel_lib//lib:tar_toolchain_type",
    )

    # TODO(3.0): Always return a list of providers.
    default_info = DefaultInfo(files = depset([out]), runfiles = ctx.runfiles([out]))
    if unused_inputs_file:
        return [
            default_info,
            OutputGroupInfo(
                # exposed for testing
                _unused_inputs_file = depset([unused_inputs_file]),
            ),
        ]
    return default_info

def _mtree_line(file, type, content = None, uid = "0", gid = "0", time = "1672560000", mode = "0755"):
    spec = [
        file,
        "uid=" + uid,
        "gid=" + gid,
        "time=" + time,
        "mode=" + mode,
        "type=" + type,
    ]
    if content:
        spec.append("content=" + content)
    return " ".join(spec)

# This function exactly same as the one from "@aspect_bazel_lib//lib:paths.bzl"
# except that it takes workspace_name directly instead of the ctx object.
# Reason is the performance of Args.add_all closures where we use this function.
# https://bazel.build/rules/lib/builtins/Args#add_all `allow_closure` explains this.
def _to_rlocation_path(file, workspace):
    if file.short_path.startswith("../"):
        return file.short_path[3:]
    return workspace + "/" + file.short_path

def _vis_encode(filename):
    # TODO(#794): correctly encode all filenames by using vis(3) (or porting it)
    return filename.replace(" ", "\\040")

def _expand(file, expander, transform = to_repository_relative_path):
    expanded = expander.expand(file)
    lines = []
    for e in expanded:
        path = transform(e)
        segments = path.split("/")
        for i in range(1, len(segments)):
            parent = "/".join(segments[:i])

            # NOTE: The mtree format treats file paths without slashes as "relative" entries.
            #       If a relative entry is a directory, then it will "change directory" to that
            #       directory, and any subsequent "relative" entries will be created inside that
            #       directory. This causes issues when there is a top-level directory that is
            #       followed by a top-level file, as the file will be created inside the directory.
            #       To avoid this, we append a slash to the directory path to make it a "full" entry.
            if i == 1:
                parent += "/"

            lines.append(_mtree_line(_vis_encode(parent), "dir"))

        lines.append(_mtree_line(_vis_encode(path), "file", content = _vis_encode(e.path)))
    return lines

def _mtree_impl(ctx):
    out = ctx.outputs.out or ctx.actions.declare_file(ctx.attr.name + ".spec")

    content = ctx.actions.args()
    content.set_param_file_format("multiline")
    content.add_all(
        ctx.files.srcs,
        map_each = _expand,
        expand_directories = True,
        uniquify = True,
    )

    for s in ctx.attr.srcs:
        default_info = s[DefaultInfo]
        if not default_info.files_to_run.runfiles_manifest:
            continue

        runfiles_dir = _calculate_runfiles_dir(default_info)
        repo_mapping = _repo_mapping_manifest(default_info.files_to_run)

        # copy workspace name here just in case to prevent ctx
        # to be transferred to execution phase.
        workspace_name = str(ctx.workspace_name)

        content.add(_mtree_line(runfiles_dir, type = "dir"))
        content.add_all(
            s.default_runfiles.empty_filenames,
            format_each = "{}/%s".format(runfiles_dir),
            # be careful about what you pass to map_each as it will carry the data structures over to execution phase.
            map_each = lambda f, e: _mtree_line(_vis_encode(f.removeprefix("external/") if f.startswith("external/") else workspace_name + "/" + f), "file"),
            allow_closure = True,
        )
        content.add_all(
            s.default_runfiles.files,
            expand_directories = True,
            uniquify = True,
            format_each = "{}/%s".format(runfiles_dir),
            # be careful about what you pass to map_each as it will carry the data structures over to execution phase.
            map_each = lambda f, e: _expand(f, e, lambda f: _to_rlocation_path(f, workspace_name)),
            allow_closure = True,
        )
        if repo_mapping != None:
            content.add(
                _mtree_line(_vis_encode(runfiles_dir + "/_repo_mapping"), "file", content = _vis_encode(repo_mapping.path)),
            )

    ctx.actions.write(out, content = content)

    return DefaultInfo(files = depset([out]), runfiles = ctx.runfiles([out]))

def _mtree_mutate_impl(ctx):
    srcs_runfiles = [
        src[DefaultInfo].default_runfiles.files
        for src in ctx.attr.srcs
    ]
    args = ctx.actions.args()

    out_mtree = ctx.outputs.out

    # Use bin directory to determine if symlink is within or outside the sandbox
    args.add("-v bin_dir={}".format(ctx.bin_dir.path))

    if ctx.attr.owner:
        args.add("-v owner={}".format(ctx.attr.owner))
    if ctx.attr.ownername:
        args.add("-v ownername={}".format(ctx.attr.ownername))
    if ctx.attr.strip_prefix:
        args.add("-v strip_prefix={}".format(ctx.attr.strip_prefix))
    if ctx.attr.package_dir:
        args.add("-v package_dir={}".format(ctx.attr.package_dir))
    if ctx.attr.mtime:
        args.add("-v mtime={}".format(ctx.attr.mtime))
    if ctx.attr.preserve_symlinks:
        args.add("-v preserve_symlinks=1")

    inputs = ctx.files.srcs[:]
    inputs.append(ctx.file.mtree)
    inputs.append(ctx.file.awk_script)
    ctx.actions.run_shell(
        command = """
        awk $@ -f {awk_script} {mtree} > {out_mtree}
        """.format(
            awk_script = ctx.file.awk_script.path,
            mtree = ctx.file.mtree.path,
            out_mtree = out_mtree.path,
        ),
        arguments = [args],
        inputs = depset(
            direct = inputs,
            transitive = srcs_runfiles,
        ),
        outputs = [out_mtree],
    )

    return [DefaultInfo(files = depset([out_mtree]))]

mtree_mutate = rule(
    implementation = _mtree_mutate_impl,
    attrs = _mutate_mtree_attrs,
)

tar_lib = struct(
    attrs = _tar_attrs,
    implementation = _tar_impl,
    mtree_attrs = _mtree_attrs,
    mtree_implementation = _mtree_impl,
    toolchain_type = TAR_TOOLCHAIN_TYPE,
    common = struct(
        accepted_tar_extensions = _ACCEPTED_EXTENSIONS,
        accepted_compression_types = _ACCEPTED_COMPRESSION_TYPES,
        compression_to_extension = _COMPRESSION_TO_EXTENSION,
        add_compression_args = _add_compression_args,
    ),
)

tar = rule(
    doc = "Rule that executes BSD `tar`. Most users should use the [`tar`](#tar) macro, rather than load this directly.",
    implementation = tar_lib.implementation,
    attrs = tar_lib.attrs,
    toolchains = [
        tar_lib.toolchain_type,
        "@aspect_bazel_lib//lib:coreutils_toolchain_type",
    ],
)
