"""write_source_files provides a workaround for the restriction that `bazel build` cannot write to the source tree.

Read more about the philosophy of writing to the source tree: <https://blog.aspect.build/bazel-can-write-to-the-source-folder>

## Usage

```starlark
load("@aspect_bazel_lib//lib:write_source_files.bzl", "write_source_files")

write_source_files(
    name = "write_foobar",
    files = {
        "foobar.json": "//some/generated:file",
    },
)
```

To update the source file, run:

```bash
bazel run //:write_foobar
```

The generated `diff_test` will fail if the file is out of date and print out instructions on
how to update it.

If the file does not exist, Bazel will fail at analysis time and print out instructions on
how to create it.

You can declare a tree of generated source file targets:

```starlark
load("@aspect_bazel_lib//lib:write_source_files.bzl", "write_source_files")

write_source_files(
    name = "write_all",
    additional_update_targets = [
        # Other write_source_files targets to run when this target is run
        "//a/b/c:write_foo",
        "//a/b:write_bar",
    ]
)
```

And update them with a single run:

```bash
bazel run //:write_all
```

When a file is out of date, you can leave a suggestion to run a target further up in the tree by specifying `suggested_update_target`.
For example,

```starlark
write_source_files(
    name = "write_foo",
    files = {
        "foo.json": ":generated-foo",
    },
    suggested_update_target = "//:write_all"
)
```

A test failure from `foo.json` being out of date will yield the following message:

```
//a/b:c:foo.json is out of date. To update this and other generated files, run:

    bazel run //:write_all

To update *only* this file, run:

    bazel run //a/b/c:write_foo
```

You can also add a more customized error message using the `diff_test_failure_message` argument:

```starlark
write_source_file(
    name = "write_foo",
    out_file = "foo.json",
    in_file = ":generated-foo",
    diff_test_failure_message = "Failed to build Foo; please run {{TARGET}} to update."
)
```

A test failure from `foo.json` being out of date will then yield:

```
Failed to build Foo; please run //a/b/c:write_foo to update.
```

If you have many `write_source_files` targets that you want to update as a group, we recommend wrapping
`write_source_files` in a macro that defaults `suggested_update_target` to the umbrella update target.

NOTE: If you run formatters or linters on your codebase, it is advised that you exclude/ignore the outputs of this
    rule from those formatters/linters so as to avoid causing collisions and failing tests.

"""

load(
    "//lib/private:write_source_file.bzl",
    _WriteSourceFileInfo = "WriteSourceFileInfo",
    _write_source_file = "write_source_file",
)

WriteSourceFileInfo = _WriteSourceFileInfo

def write_source_files(
        name,
        files = {},
        executable = False,
        additional_update_targets = [],
        suggested_update_target = None,
        diff_test = True,
        diff_test_failure_message = "{{DEFAULT_MESSAGE}}",
        diff_args = [],
        file_missing_failure_message = "{{DEFAULT_MESSAGE}}",
        check_that_out_file_exists = True,
        **kwargs):
    """Write one or more files and/or directories to the source tree.

    By default, `diff_test` targets are generated that ensure the source tree files and/or directories to be written to
    are up to date and the rule also checks that all source tree files and/or directories to be written to exist.
    To disable the exists check and up-to-date tests set `diff_test` to `False`.

    Args:
        name: Name of the runnable target that creates or updates the source tree files and/or directories.

        files: A dict where the keys are files or directories in the source tree to write to and the values are labels
            pointing to the desired content, typically file or directory outputs of other targets.

            Destination files and directories must be within the same containing Bazel package as this target if
            `check_that_out_file_exists` is True. See `check_that_out_file_exists` docstring for more info.

        executable: Whether source tree files written should be made executable.

            This applies to all source tree files written by this target. This attribute is not propagated to `additional_update_targets`.

            To set different executable permissions on different source tree files use multiple `write_source_files` targets.

        additional_update_targets: List of other `write_source_files` or `write_source_file` targets to call in the same run.

        suggested_update_target: Label of the `write_source_files` or `write_source_file` target to suggest running when files are out of date.

        diff_test: Test that the source tree files and/or directories exist and are up to date.

        diff_test_failure_message: Text to print when the diff test fails, with templating options for
            relevant targets.

            Substitutions are performed on the failure message, with the following substitutions being available:

            `{{DEFAULT_MESSAGE}}`: Prints the default error message, listing the target(s) that
              may be run to update the file(s).

            `{{TARGET}}`: The target to update the individual file that does not match in the
              diff test.

            `{{SUGGESTED_UPDATE_TARGET}}`: The suggested_update_target if specified, or the
              target which will update all of the files which do not match.

        diff_args: Arguments to pass to the `diff` command. (Ignored on Windows)

        file_missing_failure_message: Text to print when the output file is missing. Subject to the same
             substitutions as diff_test_failure_message.

        check_that_out_file_exists: Test that each output file exists and print a helpful error message if it doesn't.

            If `True`, destination files and directories must be in the same containing Bazel package as the target since the underlying
            mechanism for this check is limited to files in the same Bazel package.

        **kwargs: Other common named parameters such as `tags` or `visibility`
    """

    single_update_target = len(files.keys()) == 1
    update_targets = []
    test_targets = []
    for i, pair in enumerate(files.items()):
        out_file, in_file = pair

        this_suggested_update_target = suggested_update_target
        if single_update_target:
            update_target_name = name
        else:
            update_target_name = "%s_%d" % (name, i)
            update_targets.append(update_target_name)
            if not this_suggested_update_target:
                this_suggested_update_target = name

        # Runnable target that writes to the out file to the source tree
        test_target = _write_source_file(
            name = update_target_name,
            in_file = in_file,
            out_file = out_file,
            executable = executable,
            additional_update_targets = additional_update_targets if single_update_target else [],
            suggested_update_target = this_suggested_update_target,
            diff_test = diff_test,
            diff_test_failure_message = diff_test_failure_message,
            diff_args = diff_args,
            file_missing_failure_message = file_missing_failure_message,
            check_that_out_file_exists = check_that_out_file_exists,
            **kwargs
        )

        if test_target:
            test_targets.append(test_target)

    if len(test_targets) > 0:
        native.test_suite(
            name = "%s_tests" % name,
            tests = test_targets,
            visibility = kwargs.get("visibility"),
            tags = kwargs.get("tags"),
        )

    if not single_update_target:
        _write_source_file(
            name = name,
            additional_update_targets = update_targets + additional_update_targets,
            suggested_update_target = suggested_update_target,
            **kwargs
        )

write_source_file = _write_source_file
