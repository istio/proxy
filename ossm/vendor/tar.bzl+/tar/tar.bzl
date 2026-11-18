"""
API for calling tar, see https://man.freebsd.org/cgi/man.cgi?tar(1)

Load from:

```starlark
load("@tar.bzl", "tar")
```

Examples
--------

Build this target to produce archive.tar:
```starlark
tar(
    name = "archive",
    srcs = ["my-file.txt"],
)
```

Mutations allow modification of the archive's structure. For example to strip the package name:

```starlark
load("@tar.bzl", "mutate", "tar")

tar(
    name = "new",
    srcs = ["my-file.txt"],
    # See arguments documented at
    # https://github.com/bazel-contrib/tar.bzl/blob/main/docs/mtree.md#mtree_mutate
    mutate = mutate(strip_prefix = package_name()),
)
```
"""

load("@bazel_lib//lib:expand_template.bzl", "expand_template")
load("@bazel_lib//lib:utils.bzl", "propagate_common_rule_attributes")
load("@bazel_skylib//lib:partial.bzl", "partial")
load("@bazel_skylib//lib:types.bzl", "types")
load("//tar/private:tar.bzl", _tar = "tar", _tar_lib = "tar_lib")
load(":mtree.bzl", "mtree_spec")

tar_rule = _tar

tar_lib = _tar_lib

def tar(name, mtree = "auto", mutate = None, include_runfiles = None, stamp = 0, **kwargs):
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
        mutate: a partially-applied `mtree_mutate` rule
        include_runfiles: When using "auto" mtree, this controls whether to include runfiles.

            If mtree is supplied as an array literal of lines, you are already hardcoding list of included files.

            When mtree is a label, you need to set [include_runfiles](https://github.com/bazel-contrib/tar.bzl/blob/main/docs/mtree.md#mtree_spec-include_runfiles) in mtree_specs.
        stamp: should mtree attribute be stamped
        **kwargs: additional named parameters to pass to `tar_rule`
    """
    mtree_target = "{}_mtree".format(name)
    if mutate and mtree != "auto":
        fail("mutate is only supported when mtree is 'auto'")

    if include_runfiles != None and mtree != "auto":
        fail("include_runfiles is only supported when mtree is 'auto', otherwise include_runfiles should be set in mtree_spec")

    if mtree == "auto":
        if include_runfiles == None:
            include_runfiles = True
        mtree_spec(
            name = mtree_target,
            srcs = kwargs.get("srcs", []),
            out = "{}.txt".format(mtree_target),
            include_runfiles = include_runfiles,
            **propagate_common_rule_attributes(kwargs)
        )
        if mutate:
            if partial.is_instance(mutate):
                mutated_mtree_target = "{}__mutated".format(name)
                partial.call(mutate, name = mutated_mtree_target, mtree = mtree_target)
                mtree_target = mutated_mtree_target
            else:
                fail("mutate must be a partial")
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
