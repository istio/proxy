"""Rule and corresponding provider that joins a label pointing to a TreeArtifact
with a path nested within that directory
"""

load("//lib:utils.bzl", _to_label = "to_label")

DirectoryPathInfo = provider(
    doc = "Joins a label pointing to a TreeArtifact with a path nested within that directory.",
    fields = {
        "directory": "a TreeArtifact (ctx.actions.declare_directory)",
        "path": "path relative to the directory",
    },
)

def _directory_path(ctx):
    if not ctx.file.directory.is_directory:
        msg = "Expected directory to be a TreeArtifact (ctx.actions.declare_directory) but {} is either a source file or does not exist.".format(ctx.file.directory)
        fail(msg)
    return [DirectoryPathInfo(path = ctx.attr.path, directory = ctx.file.directory)]

directory_path = rule(
    doc = """Provide DirectoryPathInfo to reference some path within a directory.

Otherwise there is no way to give a Bazel label for it.""",
    implementation = _directory_path,
    attrs = {
        "directory": attr.label(
            doc = "a TreeArtifact (ctx.actions.declare_directory)",
            mandatory = True,
            allow_single_file = True,
        ),
        "path": attr.string(
            doc = "path relative to the directory",
            mandatory = True,
        ),
    },
    provides = [DirectoryPathInfo],
)

def make_directory_path(name, directory, path, **kwargs):
    """Helper function to generate a directory_path target and return its label.

    Args:
        name: unique name for the generated `directory_path` target
        directory: `directory` attribute passed to generated `directory_path` target
        path: `path` attribute passed to generated `directory_path` target
        **kwargs: parameters to pass to generated `output_files` target

    Returns:
        The label `name`
    """
    directory_path(
        name = name,
        directory = directory,
        path = path,
        **kwargs
    )
    return _to_label(name)

def make_directory_paths(name, dict, **kwargs):
    """Helper function to convert a dict of directory to path mappings to directory_path targets and labels.

    For example,

    ```
    make_directory_paths("my_name", {
        "//directory/artifact:target_1": "file/path",
        "//directory/artifact:target_2": ["file/path1", "file/path2"],
    })
    ```

    generates the targets,

    ```
    directory_path(
        name = "my_name_0",
        directory = "//directory/artifact:target_1",
        path = "file/path"
    )

    directory_path(
        name = "my_name_1",
        directory = "//directory/artifact:target_2",
        path = "file/path1"
    )

    directory_path(
        name = "my_name_2",
        directory = "//directory/artifact:target_2",
        path = "file/path2"
    )
    ```

    and the list of targets is returned,

    ```
    [
        "my_name_0",
        "my_name_1",
        "my_name_2",
    ]
    ```

    Args:
        name: The target name to use for the generated targets & labels.

            The names are generated as zero-indexed `name + "_" + i`

        dict: The dictionary of directory keys to path or path list values.
        **kwargs: additional parameters to pass to each generated target

    Returns:
        The label of the generated `directory_path` targets named `name + "_" + i`
    """
    labels = []
    pairs = []
    for directory, val in dict.items():
        if type(val) == "list":
            for path in val:
                pairs.append((directory, path))
        elif type(val) == "string":
            pairs.append((directory, val))
        else:
            fail("Value must be a list or string")
    for i, pair in enumerate(pairs):
        directory, path = pair
        labels.append(make_directory_path(
            "%s_%d" % (name, i),
            directory,
            path,
            **kwargs
        ))
    return labels
