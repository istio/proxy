"""Implementation of whl_filegroup rule."""

def _whl_filegroup_impl(ctx):
    out_dir = ctx.actions.declare_directory(ctx.attr.name)
    ctx.actions.run(
        outputs = [out_dir],
        inputs = [ctx.file.whl],
        arguments = [
            ctx.file.whl.path,
            out_dir.path,
            ctx.attr.pattern,
        ],
        executable = ctx.executable._extract_wheel_files_tool,
        mnemonic = "PyExtractWheelFiles",
        progress_message = "Extracting %s files from %s" % (ctx.attr.pattern, ctx.file.whl.short_path),
    )
    return [DefaultInfo(
        files = depset([out_dir]),
        runfiles = ctx.runfiles(files = [out_dir] if ctx.attr.runfiles else []),
    )]

whl_filegroup = rule(
    _whl_filegroup_impl,
    doc = """Extract files matching a regular expression from a wheel file.

An empty pattern will match all files.

Example usage:
```starlark
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_python//python:pip.bzl", "whl_filegroup")

whl_filegroup(
    name = "numpy_includes",
    pattern = "numpy/core/include/numpy",
    whl = "@pypi//numpy:whl",
)

cc_library(
    name = "numpy_headers",
    hdrs = [":numpy_includes"],
    includes = ["numpy_includes/numpy/core/include"],
    deps = ["@rules_python//python/cc:current_py_cc_headers"],
)
```
""",
    attrs = {
        "pattern": attr.string(default = "", doc = "Only file paths matching this regex pattern will be extracted."),
        "runfiles": attr.bool(default = False, doc = "Whether to include the output TreeArtifact in this target's runfiles."),
        "whl": attr.label(mandatory = True, allow_single_file = True, doc = "The wheel to extract files from."),
        "_extract_wheel_files_tool": attr.label(
            default = Label("//python/private/whl_filegroup:extract_wheel_files"),
            cfg = "exec",
            executable = True,
        ),
    },
)
