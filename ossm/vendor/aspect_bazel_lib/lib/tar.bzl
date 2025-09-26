"""General-purpose rule to create tar archives.

Unlike [pkg_tar from rules_pkg](https://github.com/bazelbuild/rules_pkg/blob/main/docs/latest.md#pkg_tar):

- It does not depend on any Python interpreter setup
- The "manifest" specification is a mature public API and uses a compact tabular format, fixing
  https://github.com/bazelbuild/rules_pkg/pull/238
- It doesn't rely custom program to produce the output, instead
  we rely on the well-known C++ program called "tar".
  Specifically, we use the BSD variant of tar since it provides a means
  of controlling mtimes, uid, symlinks, etc.

We also provide full control for tar'ring binaries including their runfiles.

The `tar` binary is hermetic and fully statically-linked.
It is fetched as a toolchain from https://github.com/aspect-build/bsdtar-prebuilt.

## Examples

See the [`tar` tests](/lib/tests/tar/BUILD.bazel) for examples of usage.

## Mutating the tar contents

The `mtree_spec` rule can be used to create an mtree manifest for the tar file.
Then you can mutate that spec using `mtree_mutate` and feed the result
as the `mtree` attribute of the `tar` rule.

For example, to set the owner uid of files in the tar, you could:

```starlark
_TAR_SRCS = ["//some:files"]

mtree_spec(
    name = "mtree",
    srcs = _TAR_SRCS,
)

mtree_mutate(
    name = "change_owner",
    mtree = ":mtree",
    owner = "1000",
)

tar(
    name = "tar",
    srcs = _TAR_SRCS,
    mtree = "change_owner",
)
```

TODO:
- Provide convenience for rules_pkg users to re-use or replace pkg_files trees
"""

load("@bazel_skylib//lib:types.bzl", "types")
load("//lib:expand_template.bzl", "expand_template")
load("//lib:utils.bzl", "propagate_common_rule_attributes")
load("//lib/private:tar.bzl", _mutate_mtree = "mtree_mutate", _tar = "tar", _tar_lib = "tar_lib")

mtree_spec = rule(
    doc = "Create an mtree specification to map a directory hierarchy. See https://man.freebsd.org/cgi/man.cgi?mtree(8)",
    implementation = _tar_lib.mtree_implementation,
    attrs = _tar_lib.mtree_attrs,
)

tar_rule = _tar

tar_lib = _tar_lib

def tar(name, mtree = "auto", stamp = 0, **kwargs):
    """Wrapper macro around [`tar_rule`](#tar_rule).

    ### Options for mtree

    mtree provides the "specification" or manifest of a tar file.
    See https://man.freebsd.org/cgi/man.cgi?mtree(8)
    Because BSD tar doesn't have a flag to set modification times to a constant,
    we must always supply an mtree input to get reproducible builds.
    See https://reproducible-builds.org/docs/archives/ for more explanation.

    1. By default, mtree is "auto" which causes the macro to create an `mtree_spec` rule.

    2. `mtree` may be supplied as an array literal of lines, e.g.

    ```
    mtree =[
        "usr/bin uid=0 gid=0 mode=0755 type=dir",
        "usr/bin/ls uid=0 gid=0 mode=0755 time=0 type=file content={}/a".format(package_name()),
    ],
    ```

    For the format of a line, see "There are four types of lines in a specification" on the man page for BSD mtree,
    https://man.freebsd.org/cgi/man.cgi?mtree(8)

    3. `mtree` may be a label of a file containing the specification lines.

    Args:
        name: name of resulting `tar_rule`
        mtree: "auto", or an array of specification lines, or a label of a file that contains the lines.
            Subject to [$(location)](https://bazel.build/reference/be/make-variables#predefined_label_variables)
            and ["Make variable"](https://bazel.build/reference/be/make-variables) substitution.
        stamp: should mtree attribute be stamped
        **kwargs: additional named parameters to pass to `tar_rule`
    """
    mtree_target = "{}_mtree".format(name)
    if mtree == "auto":
        mtree_spec(
            name = mtree_target,
            srcs = kwargs.get("srcs", []),
            out = "{}.txt".format(mtree_target),
            **propagate_common_rule_attributes(kwargs)
        )
    elif types.is_list(mtree):
        expand_template(
            name = mtree_target,
            out = "{}.txt".format(mtree_target),
            data = kwargs.get("srcs", []),
            # Ensure there's a trailing newline, as bsdtar will ignore a last line without one
            template = ["#mtree", "{content}", ""],
            substitutions = {
                # expand_template only expands strings in "substitutions" dict. Here
                # we expand mtree and then replace the template with expanded mtree.
                "{content}": "\n".join(mtree),
            },
            stamp = stamp,
            **propagate_common_rule_attributes(kwargs)
        )
    else:
        mtree_target = mtree

    tar_rule(
        name = name,
        mtree = mtree_target,
        **kwargs
    )

def mtree_mutate(
        name,
        mtree,
        srcs = None,
        preserve_symlinks = False,
        strip_prefix = None,
        package_dir = None,
        mtime = None,
        owner = None,
        ownername = None,
        awk_script = Label("@aspect_bazel_lib//lib/private:modify_mtree.awk"),
        **kwargs):
    """Modify metadata in an mtree file.

    Args:
        name: name of the target, output will be `[name].mtree`.
        mtree: input mtree file, typically created by `mtree_spec`.
        srcs: list of files to resolve symlinks for.
        preserve_symlinks: `EXPERIMENTAL!` We may remove or change it at any point without further notice. Flag to determine whether to preserve symlinks in the tar.
        strip_prefix: prefix to remove from all paths in the tar. Files and directories not under this prefix are dropped.
        package_dir: directory prefix to add to all paths in the tar.
        mtime: new modification time for all entries.
        owner: new uid for all entries.
        ownername: new uname for all entries.
        awk_script: may be overridden to change the script containing the modification logic.
        **kwargs: additional named parameters to genrule
    """
    if preserve_symlinks and not srcs:
        fail("preserve_symlinks requires srcs to be set in order to resolve symlinks")

    # Check if srcs is of type list
    if srcs and not types.is_list(srcs):
        srcs = [srcs]
    _mutate_mtree(
        name = name,
        mtree = mtree,
        srcs = srcs,
        preserve_symlinks = preserve_symlinks,
        strip_prefix = strip_prefix,
        package_dir = package_dir,
        mtime = str(mtime) if mtime else None,
        owner = owner,
        ownername = ownername,
        awk_script = awk_script,
        out = "{}.mtree".format(name),
        **kwargs
    )
