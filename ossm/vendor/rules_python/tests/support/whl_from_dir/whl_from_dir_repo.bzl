"""Creates a whl file from a directory tree.

Used to test wheels. Avoids checking in prebuilt files and their associated
security risks.
"""

load("//python/private:repo_utils.bzl", "repo_utils")  # buildifier: disable=bzl-visibility

def _whl_from_dir_repo(rctx):
    root = rctx.path(rctx.attr.root).dirname
    rctx.watch_tree(root)

    output = rctx.path(rctx.attr.output)
    repo_utils.execute_checked(
        rctx,
        # cd to root so zip recursively takes everything there.
        working_directory = str(root),
        op = "WhlFromDir",
        arguments = [
            "zip",
            "-0",  # Skip compressing
            "-X",  # Don't store file time or metadata
            str(output),
            "-r",
            ".",
        ],
    )
    rctx.file("BUILD.bazel", 'exports_files(glob(["*"]))')

whl_from_dir_repo = repository_rule(
    implementation = _whl_from_dir_repo,
    attrs = {
        "output": attr.string(
            doc = """
Output file name to write. Should match the wheel filename format:
`pkg-version-pyversion-abi-platform.whl`. Typically a value like
`mypkg-1.0-any-none-any.whl` is whats used for testing.

For the full format, see
https://packaging.python.org/en/latest/specifications/binary-distribution-format/#file-name-convention
""",
        ),
        "root": attr.label(
            doc = """
A file whose directory will be put into the output wheel. All files
are included verbatim.
            """,
        ),
    },
)
