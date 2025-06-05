"""output_files implementation
"""

load("//lib:utils.bzl", _to_label = "to_label")

def _output_files(ctx):
    files = []
    files_depset = depset()
    if ctx.attr.output_group:
        if OutputGroupInfo not in ctx.attr.target:
            msg = "%s output_group is specified but %s does not provide an OutputGroupInfo" % (ctx.attr.output_group, ctx.attr.target)
            fail(msg)
        if ctx.attr.output_group not in ctx.attr.target[OutputGroupInfo]:
            msg = "%s output_group is specified but %s does not provide this output group" % (ctx.attr.output_group, ctx.attr.target)
            fail(msg)
        files_depset = ctx.attr.target[OutputGroupInfo][ctx.attr.output_group]
    else:
        files_depset = ctx.attr.target[DefaultInfo].files

    files_list = files_depset.to_list()

    for path in ctx.attr.paths:
        file = _find_short_path_in_files_list(files_list, path)
        if not file:
            if ctx.attr.output_group:
                msg = "%s file not found within the %s output group of %s" % (path, ctx.attr.output_group, ctx.attr.target)
            else:
                msg = "%s file not found within the DefaultInfo of %s" % (path, ctx.attr.target)
            fail(msg)
        files.append(file)
    return [DefaultInfo(
        files = depset(direct = files),
        runfiles = ctx.runfiles(files = files),
    )]

output_files = rule(
    doc = "A rule that provides file(s) specific via DefaultInfo from a given target's DefaultInfo or OutputGroupInfo",
    implementation = _output_files,
    attrs = {
        "target": attr.label(
            doc = "the target to look in for requested paths in its' DefaultInfo or OutputGroupInfo",
            mandatory = True,
        ),
        "paths": attr.string_list(
            doc = "the paths of the file(s), relative to their roots, to provide via DefaultInfo from the given target's DefaultInfo or OutputGroupInfo",
            mandatory = True,
            allow_empty = False,
        ),
        "output_group": attr.string(
            doc = "if set, we look in the specified output group for paths instead of DefaultInfo",
        ),
    },
    provides = [DefaultInfo],
)

def make_output_files(name, target, paths, **kwargs):
    """Helper function to generate a output_files target and return its label.

    Args:
        name: unique name for the generated `output_files` target
        target: `target` attribute passed to generated `output_files` target
        paths: `paths` attribute passed to generated `output_files` target
        **kwargs: parameters to pass to generated `output_files` target

    Returns:
        The label `name`
    """
    output_files(
        name = name,
        target = target,
        paths = paths,
        **kwargs
    )
    return _to_label(name)

def _find_short_path_in_files_list(files_list, short_path):
    """Helper function find a file in a DefaultInfo by short path

    Args:
        files_list: a list of files
        short_path: the short path (path relative to root) to search for
    Returns:
        The File if found else None
    """
    for file in files_list:
        if file.short_path == short_path:
            return file
    return None
