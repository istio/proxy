"write_source_file implementation"

load(":diff_test.bzl", _diff_test = "diff_test")
load(":directory_path.bzl", "DirectoryPathInfo")
load(":fail_with_message_test.bzl", "fail_with_message_test")
load(":utils.bzl", "utils")

WriteSourceFileInfo = provider(
    "Provider for write_source_file targets",
    fields = {
        "executable": "Executable that updates the source files",
    },
)

def write_source_file(
        name,
        in_file = None,
        out_file = None,
        executable = False,
        additional_update_targets = [],
        suggested_update_target = None,
        diff_test = True,
        diff_test_failure_message = "{{DEFAULT_MESSAGE}}",
        file_missing_failure_message = "{{DEFAULT_MESSAGE}}",
        diff_args = [],
        check_that_out_file_exists = True,
        verbosity = "full",
        **kwargs):
    """Write a file or directory to the source tree.

    By default, a `diff_test` target ("{name}_test") is generated that ensure the source tree file or directory to be written to
    is up to date and the rule also checks that the source tree file or directory to be written to exists.
    To disable the exists check and up-to-date test set `diff_test` to `False`.

    Args:
        name: Name of the runnable target that creates or updates the source tree file or directory.

        in_file: File or directory to use as the desired content to write to `out_file`.

            This is typically a file or directory output of another target. If `in_file` is a directory then entire directory contents are copied.

        out_file: The file or directory to write to in the source tree.

            The output file or directory must be within the same containing Bazel package as this target if `check_that_out_file_exists` is `True`.
            See `check_that_out_file_exists` docstring for more info.

        executable: Whether source tree file or files within the source tree directory written should be made executable.

        additional_update_targets: List of other `write_source_files` or `write_source_file` targets to call in the same run.

        suggested_update_target: Label of the `write_source_files` or `write_source_file` target to suggest running when files are out of date.

        diff_test: Test that the source tree file or directory exist and is up to date.

        diff_test_failure_message: Text to print when the diff test fails, with templating options for
            relevant targets.

            Substitutions are performed on the failure message, with the following substitutions being available:

            `{{DEFAULT_MESSAGE}}`: Prints the default error message, listing the target(s) that
              may be run to update the file(s).

            `{{TARGET}}`: The target to update the individual file that does not match in the
              diff test.

            `{{SUGGESTED_UPDATE_TARGET}}`: The suggested_update_target if specified.

        file_missing_failure_message: Text to print when the output file is missing. Subject to the same
             substitutions as diff_test_failure_message.

        diff_args: Arguments to pass to the `diff` command. (Ignored on Windows)

        check_that_out_file_exists: Test that the output file exists and print a helpful error message if it doesn't.

            If `True`, the output file or directory must be in the same containing Bazel package as the target since the underlying mechanism
            for this check is limited to files in the same Bazel package.

        verbosity: Verbosity of message when the copy target is run. One of `full`, `short`, `quiet`.

        **kwargs: Other common named parameters such as `tags` or `visibility`

    Returns:
        Name of the generated test target if requested, otherwise None.
    """
    if out_file:
        if not in_file:
            fail("in_file must be specified if out_file is set")

    if in_file:
        if not out_file:
            fail("out_file must be specified if in_file is set")

    if out_file:
        out_file = utils.to_label(out_file)

        if utils.is_external_label(out_file):
            msg = "out file {} must be in the user workspace".format(out_file)
            fail(msg)

        if check_that_out_file_exists and out_file.package != native.package_name():
            msg = "out file {} (in package '{}') must be a source file within the target's package: '{}'; set check_that_out_file_exists to False to work-around this requirement".format(out_file, out_file.package, native.package_name())
            fail(msg)

    _write_source_file(
        name = name,
        in_file = in_file,
        out_file = str(out_file) if out_file else None,
        executable = executable,
        additional_update_targets = additional_update_targets,
        verbosity = verbosity,
        **kwargs
    )

    if not in_file or not out_file or not diff_test:
        return None

    out_file_missing = check_that_out_file_exists and _is_file_missing(out_file)
    test_target_name = "%s_test" % name

    update_target_string = "//%s:%s" % (native.package_name(), name)
    suggested_update_target_string = str(utils.to_label(suggested_update_target)) if suggested_update_target else None

    if out_file_missing:
        if suggested_update_target == None:
            default_message = """

%s does not exist. To create & update this file, run:

    bazel run %s

""" % (out_file, update_target_string)
        else:
            default_message = """

%s does not exist. To create & update this and other generated files, run:

    bazel run %s

To create an update *only* this file, run:

    bazel run %s

""" % (out_file, suggested_update_target_string, update_target_string)

        message = _do_diff_test_message_replacements(file_missing_failure_message, default_message, update_target_string, suggested_update_target_string)

        # Stamp out a test that fails with a helpful message when the source file doesn't exist.
        # Note that we cannot simply call fail() here since it will fail during the analysis
        # phase and prevent the user from calling bazel run //update/the:file.
        fail_with_message_test(
            name = test_target_name,
            message = message,
            visibility = kwargs.get("visibility"),
            tags = kwargs.get("tags"),
            size = "small",
        )
    else:
        if suggested_update_target == None:
            default_message = """

%s is out of date. To update this file, run:

    bazel run %s

""" % (out_file, update_target_string)
        else:
            default_message = """

%s is out of date. To update this and other generated files, run:

    bazel run %s

To update *only* this file, run:

    bazel run %s

""" % (out_file, suggested_update_target_string, update_target_string)

        message = _do_diff_test_message_replacements(diff_test_failure_message, default_message, update_target_string, suggested_update_target_string)

        # Stamp out a diff test the check that the source file is up to date
        _diff_test(
            name = test_target_name,
            file1 = in_file,
            file2 = out_file,
            failure_message = message,
            diff_args = diff_args,
            **kwargs
        )

    return test_target_name

_write_source_file_attrs = {
    "in_file": attr.label(allow_files = True, mandatory = False),
    # out_file is intentionally an attr.string() and not a attr.label(). This is so that
    # bazel query 'kind("source file", deps(//path/to:target))' does not return
    # out_file in the list of source file deps. ibazel uses this query to determine
    # which source files to watch so if the out_file is returned then ibazel watches
    # and it goes into an infinite update, notify loop when running this target.
    # See https://github.com/bazel-contrib/bazel-lib/pull/52 for more context.
    "out_file": attr.string(mandatory = False),
    "executable": attr.bool(),
    # buildifier: disable=attr-cfg
    "additional_update_targets": attr.label_list(
        # Intentionally use the target platform since the target is always meant to be `bazel run`
        # on the host machine but we don't want to transition it to the host platform and have the
        # generated file rebuilt in a separate output tree. Target platform should always be equal
        # to the host platform when using `write_source_files`.
        cfg = "target",
        mandatory = False,
        providers = [WriteSourceFileInfo],
    ),
    "verbosity": attr.string(
        values = ["full", "short", "quiet"],
    ),
    "_windows_constraint": attr.label(default = "@platforms//os:windows"),
    "_macos_constraint": attr.label(default = "@platforms//os:macos"),
}

def _write_source_file_sh(ctx, paths):
    is_macos = ctx.target_platform_has_constraint(ctx.attr._macos_constraint[platform_common.ConstraintValueInfo])

    updater = ctx.actions.declare_file(
        ctx.label.name + "_update.sh",
    )

    additional_update_scripts = []
    for target in ctx.attr.additional_update_targets:
        additional_update_scripts.append(target[WriteSourceFileInfo].executable)

    contents = ["""#!/usr/bin/env bash
set -o errexit -o nounset -o pipefail
runfiles_dir=$PWD
# BUILD_WORKSPACE_DIRECTORY not set when running as a test, uses the sandbox instead
if [[ ! -z "${BUILD_WORKSPACE_DIRECTORY:-}" ]]; then
    cd "$BUILD_WORKSPACE_DIRECTORY"
fi"""]

    if ctx.attr.executable:
        executable_file = "chmod +x \"$out\""
        executable_dir = "chmod -R +x \"$out\""
    else:
        executable_file = "chmod -x \"$out\""
        if is_macos:
            # -x+X doesn't work on macos so we have to find files and remove the execute bits only from those
            executable_dir = "find \"$out\" -type f | xargs chmod -x"
        else:
            # Remove execute/search bit recursively from files bit not directories: https://superuser.com/a/434418
            executable_dir = "chmod -R -x+X \"$out\""

    progress_message_dir = ""
    progress_message_file = ""
    if ctx.attr.verbosity == "full":
        progress_message_dir = "echo \"Copying directory $in to $out in $PWD\""
        progress_message_file = "echo \"Copying file $in to $out in $PWD\""
    elif ctx.attr.verbosity == "short":
        progress_message_dir = "echo \"Updating directory $out\""
        progress_message_file = "echo \"Updating file $out\""

    for in_path, out_path in paths:
        contents.append("""
in=$runfiles_dir/{in_path}
out={out_path}

mkdir -p "$(dirname "$out")"
if [[ -f "$in" ]]; then
    {progress_message_file}
    # in case `cp` from previous command was terminated midway which can result in read-only files/dirs
    chmod -R +w "$out" > /dev/null 2>&1 || true
    rm -Rf "$out"
    cp -f "$in" "$out"
    # cp should make the file writable but call chmod anyway as a defense in depth
    chmod +w "$out"
    # cp should make the file not-executable but set the desired execute bit in both cases as a defense in depth
    {executable_file}
else
    {progress_message_dir}
    # in case `cp` from previous command was terminated midway which can result in read-only files/dirs
    chmod -R +w "$out" > /dev/null 2>&1 || true
    rm -Rf "$out"/{{*,.[!.]*}}
    mkdir -p "$out"
    cp -fRL "$in"/. "$out"
    chmod -R +w "$out"
    {executable_dir}
fi
""".format(
            in_path = in_path,
            out_path = out_path,
            executable_file = executable_file,
            executable_dir = executable_dir,
            progress_message_dir = progress_message_dir,
            progress_message_file = progress_message_file,
        ))

    contents.extend([
        "cd \"$runfiles_dir\"",
        "# Run the update scripts for all write_source_file deps",
    ])
    for update_script in additional_update_scripts:
        contents.append("./\"{update_script}\"".format(update_script = update_script.short_path))

    ctx.actions.write(
        output = updater,
        is_executable = True,
        content = "\n".join(contents),
    )

    return updater

def _write_source_file_bat(ctx, paths):
    updater = ctx.actions.declare_file(
        ctx.label.name + "_update.bat",
    )

    additional_update_scripts = []
    for target in ctx.attr.additional_update_targets:
        if target[DefaultInfo].files_to_run and target[DefaultInfo].files_to_run.executable:
            additional_update_scripts.append(target[DefaultInfo].files_to_run.executable)
        else:
            fail("additional_update_targets target %s does not provide an executable")

    contents = ["""@rem @generated by @aspect_bazel_lib//:lib/private:write_source_file.bzl
@echo off
set runfiles_dir=%cd%
if defined BUILD_WORKSPACE_DIRECTORY (
    cd /d %BUILD_WORKSPACE_DIRECTORY%
)"""]

    progress_message = ""
    if ctx.attr.verbosity == "full":
        progress_message = "echo Copying %in% to %out% in %cd%"
    elif ctx.attr.verbosity == "short":
        progress_message = "echo Updating %out%"

    for in_path, out_path in paths:
        contents.append("""
set in=%runfiles_dir%\\{in_path}
set out={out_path}

if not defined BUILD_WORKSPACE_DIRECTORY (
    @rem Because there's no sandboxing in windows, if we copy over the target
    @rem file's symlink it will get copied back into the source directory
    @rem during tests. Work around this in tests by deleting the target file
    @rem symlink before copying over it.
    del %out%
)

{progress_message}

if exist "%in%\\*" (
    mkdir "%out%" >NUL 2>NUL
    robocopy "%in%" "%out%" /E >NUL
) else (
    copy %in% %out% >NUL
)
""".format(
            in_path = in_path.replace("/", "\\"),
            out_path = out_path.replace("/", "\\"),
            progress_message = progress_message,
        ))

    contents.extend([
        "cd /d %runfiles_dir%",
        "@rem Run the update scripts for all write_source_file deps",
    ])
    for update_script in additional_update_scripts:
        contents.append("call {update_script}".format(update_script = update_script.short_path))

    ctx.actions.write(
        output = updater,
        is_executable = True,
        content = "\n".join(contents).replace("\n", "\r\n"),
    )
    return updater

def _write_source_file_impl(ctx):
    is_windows = ctx.target_platform_has_constraint(ctx.attr._windows_constraint[platform_common.ConstraintValueInfo])

    out_file = Label(ctx.attr.out_file) if ctx.attr.out_file else None

    if out_file and not ctx.attr.in_file:
        fail("in_file must be specified if out_file is set")
    if ctx.attr.in_file and not out_file:
        fail("out_file must be specified if in_file is set")

    paths = []
    runfiles = []

    if ctx.attr.in_file and out_file:
        if DirectoryPathInfo in ctx.attr.in_file:
            in_path = "/".join([
                ctx.attr.in_file[DirectoryPathInfo].directory.short_path,
                ctx.attr.in_file[DirectoryPathInfo].path,
            ])
            runfiles.append(ctx.attr.in_file[DirectoryPathInfo].directory)
        elif len(ctx.files.in_file) == 0:
            msg = "in file {} must provide files".format(ctx.attr.in_file.label)
            fail(msg)
        elif len(ctx.files.in_file) == 1:
            in_path = ctx.files.in_file[0].short_path
        else:
            msg = "in file {} must be a single file or a target that provides a DirectoryPathInfo".format(ctx.attr.in_file.label)
            fail(msg)

        out_path = "/".join([out_file.package, out_file.name]) if out_file.package else out_file.name
        paths.append((in_path, out_path))

    if is_windows:
        updater = _write_source_file_bat(ctx, paths)
    else:
        updater = _write_source_file_sh(ctx, paths)

    runfiles = ctx.runfiles(
        files = runfiles,
        transitive_files = ctx.attr.in_file.files if ctx.attr.in_file else None,
    )
    runfiles = runfiles.merge_all(
        [dep[DefaultInfo].default_runfiles for dep in ctx.attr.additional_update_targets],
    )

    return [
        DefaultInfo(
            executable = updater,
            runfiles = runfiles,
        ),
        WriteSourceFileInfo(
            executable = updater,
        ),
    ]

_write_source_file = rule(
    attrs = _write_source_file_attrs,
    implementation = _write_source_file_impl,
    executable = True,
)

def _is_file_missing(label):
    """Check if a file is missing by passing its relative path through a glob()

    Args
        label: the file's label
    """
    file_abs = "%s/%s" % (label.package, label.name)
    file_rel = file_abs[len(native.package_name()) + 1:]
    file_glob = native.glob([file_rel], exclude_directories = 0, allow_empty = True)

    # Check for subpackages in case the expected output contains BUILD files,
    # the above files glob will return empty.
    subpackage_glob = []
    if hasattr(native, "subpackages"):
        subpackage_glob = native.subpackages(include = [file_rel], allow_empty = True)

    return len(file_glob) == 0 and len(subpackage_glob) == 0

def _do_diff_test_message_replacements(message, default_message, target, suggested_update_target):
    """Constructs the diff test failures message from the provided template.

     Replaces the {{DEFAULT_MESSAGE}}, {{TARGET}}, and {{SUGGESTED_UPDATE_TARGET}} strings in
     message with the corresponding arguments.

     Args:
         message: The user-provided message to do template replacement on.
         default_message: The message to fill in for the {{DEFAULT_MESSAGE}} parameter.
         target: The string to fill in for the {{TARGET}} parameter.
         suggested_update_target: The string to fill in for the {{SUGGESTED_UPDATE_TARGET}} parameter."""
    return message.replace("{{DEFAULT_MESSAGE}}", default_message).replace("{{TARGET}}", target).replace("{{SUGGESTED_UPDATE_TARGET}}", suggested_update_target if suggested_update_target else "")
