"""Test rules for `current_*_files` targets"""

def _current_toolchain_files_test_impl(ctx):
    runfiles_files = []
    if ctx.attr.kind == "files":
        manifest = ctx.actions.declare_file("{}.files.manifest".format(
            ctx.label.name,
        ))
        ctx.actions.write(
            output = manifest,
            content = "\n".join([file.short_path for file in ctx.files.tool]),
        )
        runfiles_files.append(manifest)
        runfiles_files.extend(ctx.files.tool)
        input = manifest
    elif ctx.attr.kind == "executable":
        tool_files = ctx.files.tool
        if len(tool_files) != 1:
            fail("Unexpected number of files provided by tool for {}: {}".format(
                ctx.label,
                tool_files,
            ))
        input = tool_files[0]
        runfiles_files.append(input)
    else:
        fail("Unexpected kind: {}".format(ctx.attr.kind))

    extension = ".{}".format(ctx.executable._test_runner.extension) if ctx.executable._test_runner.extension else ""
    test_runner = ctx.actions.declare_file("{}{}".format(ctx.label.name, extension))
    ctx.actions.symlink(
        output = test_runner,
        target_file = ctx.executable._test_runner,
    )

    runfiles = ctx.runfiles(files = runfiles_files)
    runfiles = runfiles.merge(ctx.attr._test_runner[DefaultInfo].default_runfiles)
    runfiles = runfiles.merge(ctx.attr.tool[DefaultInfo].default_runfiles)

    return [
        DefaultInfo(
            runfiles = runfiles,
            executable = test_runner,
        ),
        RunEnvironmentInfo(
            environment = {
                "CURRENT_TOOLCHAIN_FILES_TEST_INPUT": input.short_path,
                "CURRENT_TOOLCHAIN_FILES_TEST_KIND": ctx.attr.kind,
                "CURRENT_TOOLCHAIN_FILES_TEST_PATTERN": ctx.attr.pattern,
            },
        ),
    ]

current_toolchain_files_test = rule(
    doc = "Test that `current_*_toolchain` tools consumable (executables are executable and filegroups contain expected sources)",
    implementation = _current_toolchain_files_test_impl,
    attrs = {
        "kind": attr.string(
            doc = "The kind of the component.",
            values = ["executable", "files"],
            mandatory = True,
        ),
        "pattern": attr.string(
            doc = (
                "A pattern used to confirm either executables produce an expected " +
                "value or lists of files contain expected contents."
            ),
            mandatory = True,
        ),
        "tool": attr.label(
            doc = "The current toolchain component.",
            allow_files = True,
            mandatory = True,
        ),
        "_test_runner": attr.label(
            doc = "A shared test runner for validating targets.",
            cfg = "exec",
            allow_files = True,
            executable = True,
            default = Label("//test/current_toolchain_files:current_toolchain_files_test_runner.sh"),
        ),
    },
    test = True,
)
