# Copyright 2020 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""A module defining clippy rules"""

load("@bazel_skylib//lib:structs.bzl", "structs")
load("//rust/private:common.bzl", "rust_common")
load(
    "//rust/private:providers.bzl",
    "CaptureClippyOutputInfo",
    "ClippyInfo",
    "ClippyOutputDiagnosticsInfo",
    "LintsInfo",
)
load(
    "//rust/private:rustc.bzl",
    "collect_deps",
    "collect_inputs",
    "construct_arguments",
    "get_error_format",
)
load(
    "//rust/private:utils.bzl",
    "determine_output_hash",
    "find_cc_toolchain",
    "find_toolchain",
)
load("//rust/settings:incompatible.bzl", "IncompatibleFlagInfo")

ClippyFlagsInfo = provider(
    doc = "Pass each value as an additional flag to clippy invocations",
    fields = {"clippy_flags": "List[string] Flags to pass to clippy"},
)

def _clippy_flag_impl(ctx):
    return ClippyFlagsInfo(clippy_flags = [f for f in ctx.build_setting_value if f != ""])

clippy_flag = rule(
    doc = (
        "Add a custom clippy flag from the command line with `--@rules_rust//rust/settings:clippy_flag`." +
        "Multiple uses are accumulated and appended after the extra_rustc_flags."
    ),
    implementation = _clippy_flag_impl,
    build_setting = config.string_list(flag = True, repeatable = True),
)

def _clippy_flags_impl(ctx):
    return ClippyFlagsInfo(clippy_flags = ctx.build_setting_value)

clippy_flags = rule(
    doc = (
        "Add custom clippy flags from the command line with `--@rules_rust//rust/settings:clippy_flags`."
    ),
    implementation = _clippy_flags_impl,
    build_setting = config.string_list(flag = True),
)

def get_clippy_ready_crate_info(target, aspect_ctx = None):
    """Check that a target is suitable for clippy and extract the `CrateInfo` provider from it.

    Args:
        target (Target): The target the aspect is running on.
        aspect_ctx (ctx, optional): The aspect's context object.

    Returns:
        CrateInfo, optional: A `CrateInfo` provider if clippy should be run or `None`.
    """

    # Ignore external targets
    if target.label.workspace_root.startswith("external"):
        return None

    # Targets with specific tags will not be formatted
    if aspect_ctx:
        ignore_tags = [
            "no_clippy",
            "no_lint",
            "nolint",
            "noclippy",
        ]
        for tag in aspect_ctx.rule.attr.tags:
            if tag.replace("-", "_").lower() in ignore_tags:
                return None

    # Obviously ignore any targets that don't contain `CrateInfo`
    if rust_common.crate_info in target:
        return target[rust_common.crate_info]
    elif rust_common.test_crate_info in target:
        return target[rust_common.test_crate_info].crate
    else:
        return None

def rust_clippy_action(ctx, clippy_executable, process_wrapper, crate_info, config, output = None, success_marker = None, cap_at_warnings = False, extra_clippy_flags = [], error_format = None, clippy_diagnostics_file = None):
    """Run clippy with the specified parameters.

    Args:
        ctx (ctx): The aspect's context object. This function should not read ctx.attr, but it might read ctx.rule.attr
        clippy_executable (File): The clippy executable to run
        process_wrapper (File): An executable process wrapper that can run clippy, usually @rules_rust//utils/process_wrapper
        crate_info (CrateInfo): The source crate information
        config (File): The clippy configuration file. Reference: https://doc.rust-lang.org/clippy/configuration.html#configuring-clippy
        output (File): The output file for clippy stdout/stderr. If None, no output will be captured
        success_marker (File): A file that will be written if clippy succeeds
        cap_at_warnings (bool): If set, it will cap all reports as warnings, allowing the build to continue even with clippy failures
        extra_clippy_flags (List[str]): A list of extra options to pass to clippy. If not set, every warnings will be turned into errors
        error_format (str): Which error format to use. Must be acceptable by rustc: https://doc.rust-lang.org/beta/rustc/command-line-arguments.html#--error-format-control-how-errors-are-produced
        clippy_diagnostics_file (File): File to output diagnostics to. If None, no diagnostics will be written

    Returns:
        None
    """
    toolchain = find_toolchain(ctx)
    cc_toolchain, feature_configuration = find_cc_toolchain(ctx)

    dep_info, build_info, _ = collect_deps(
        deps = crate_info.deps.to_list(),
        proc_macro_deps = crate_info.proc_macro_deps.to_list(),
        aliases = crate_info.aliases,
    )

    # Gather the necessary rust flags to apply lints, if they were provided.
    clippy_flags = []
    lint_files = []
    if hasattr(ctx.rule.attr, "lint_config") and ctx.rule.attr.lint_config:
        clippy_flags = clippy_flags + \
                       ctx.rule.attr.lint_config[LintsInfo].clippy_lint_flags + \
                       ctx.rule.attr.lint_config[LintsInfo].rustc_lint_flags
        lint_files = lint_files + \
                     ctx.rule.attr.lint_config[LintsInfo].clippy_lint_files + \
                     ctx.rule.attr.lint_config[LintsInfo].rustc_lint_files

    compile_inputs, out_dir, build_env_files, build_flags_files, linkstamp_outs, ambiguous_libs = collect_inputs(
        ctx,
        ctx.rule.file,
        ctx.rule.files,
        # Clippy doesn't need to invoke transitive linking, therefore doesn't need linkstamps.
        depset([]),
        toolchain,
        cc_toolchain,
        feature_configuration,
        crate_info,
        dep_info,
        build_info,
        lint_files,
    )

    if clippy_diagnostics_file:
        crate_info_dict = structs.to_dict(crate_info)
        crate_info_dict["rustc_output"] = clippy_diagnostics_file
        crate_info = rust_common.create_crate_info(**crate_info_dict)

    args, env = construct_arguments(
        ctx = ctx,
        attr = ctx.rule.attr,
        file = ctx.file,
        toolchain = toolchain,
        tool_path = clippy_executable.path,
        cc_toolchain = cc_toolchain,
        feature_configuration = feature_configuration,
        crate_info = crate_info,
        dep_info = dep_info,
        linkstamp_outs = linkstamp_outs,
        ambiguous_libs = ambiguous_libs,
        output_hash = determine_output_hash(crate_info.root, ctx.label),
        rust_flags = [],
        out_dir = out_dir,
        build_env_files = build_env_files,
        build_flags_files = build_flags_files,
        emit = ["dep-info", "metadata"],
        skip_expanding_rustc_env = True,
        use_json_output = bool(clippy_diagnostics_file),
        error_format = error_format,
    )

    if crate_info.is_test:
        args.rustc_flags.add("--test")

    # Then append the clippy flags specified from the command line, so they override what is
    # specified on the library.
    clippy_flags += extra_clippy_flags

    outputs = []

    if output != None:
        args.process_wrapper_flags.add("--stderr-file", output)
        outputs.append(output)

    if success_marker != None:
        args.process_wrapper_flags.add("--touch-file", success_marker)
        outputs.append(success_marker)

    if clippy_flags or lint_files:
        args.rustc_flags.add_all(clippy_flags)
    else:
        # The user didn't provide any clippy flags explicitly so we apply conservative defaults.

        # Turn any warnings from clippy or rustc into an error, as otherwise
        # Bazel will consider the execution result of the aspect to be "success",
        # and Clippy won't be re-triggered unless the source file is modified.
        args.rustc_flags.add("-Dwarnings")

    if cap_at_warnings:
        # If we are capturing the output, we want the build system to be able to keep going
        # and consume the output. Some clippy lints are denials, so we cap everything at warn.
        args.rustc_flags.add("--cap-lints=warn")

    # Upstream clippy requires one of these two filenames or it silently uses
    # the default config. Enforce the naming so users are not confused.
    valid_config_file_names = [".clippy.toml", "clippy.toml"]
    if config.basename not in valid_config_file_names:
        fail("The clippy config file must be named one of: {}".format(valid_config_file_names))
    env["CLIPPY_CONF_DIR"] = "${{pwd}}/{}".format(config.dirname)
    compile_inputs = depset([config], transitive = [compile_inputs])

    ctx.actions.run(
        executable = process_wrapper,
        inputs = compile_inputs,
        outputs = outputs + [x for x in [clippy_diagnostics_file] if x],
        env = env,
        tools = [clippy_executable],
        arguments = args.all,
        mnemonic = "Clippy",
        progress_message = "Clippy %{label}",
        toolchain = "@rules_rust//rust:toolchain_type",
    )

def _clippy_aspect_impl(target, ctx):
    # Exit early if a target already has a clippy output group. This
    # can be useful for rules which always want to inhibit clippy.
    if OutputGroupInfo in target:
        if hasattr(target[OutputGroupInfo], "clippy_checks"):
            return []

    crate_info = get_clippy_ready_crate_info(target, ctx)
    if not crate_info:
        return [ClippyInfo(output = depset([]))]

    toolchain = find_toolchain(ctx)

    # For remote execution purposes, the clippy_out file must be a sibling of crate_info.output
    # or rustc may fail to create intermediate output files because the directory does not exist.
    if ctx.attr._capture_output[CaptureClippyOutputInfo].capture_output:
        clippy_out = ctx.actions.declare_file(ctx.label.name + ".clippy.out", sibling = crate_info.output)
        clippy_success_marker = None
    else:
        # A marker file indicating clippy has executed successfully.
        # This file is necessary because "ctx.actions.run" mandates an output.
        clippy_success_marker = ctx.actions.declare_file(ctx.label.name + ".clippy.ok", sibling = crate_info.output)
        clippy_out = None

    use_clippy_error_format = ctx.attr._incompatible_change_clippy_error_format[IncompatibleFlagInfo].enabled
    error_format = get_error_format(
        ctx.attr,
        "_clippy_error_format" if use_clippy_error_format else "_error_format",
    )

    clippy_flags = ctx.attr._clippy_flags[ClippyFlagsInfo].clippy_flags

    if hasattr(ctx.attr, "_clippy_flag"):
        clippy_flags = clippy_flags + ctx.attr._clippy_flag[ClippyFlagsInfo].clippy_flags

    clippy_diagnostics = None
    if ctx.attr._clippy_output_diagnostics[ClippyOutputDiagnosticsInfo].output_diagnostics:
        clippy_diagnostics = ctx.actions.declare_file(ctx.label.name + ".clippy.diagnostics", sibling = crate_info.output)

    # Run clippy using the extracted function
    rust_clippy_action(
        ctx = ctx,
        clippy_executable = toolchain.clippy_driver,
        process_wrapper = ctx.executable._process_wrapper,
        crate_info = crate_info,
        config = ctx.file._config,
        output = clippy_out,
        cap_at_warnings = clippy_out != None,  # If we're capturing output, we want the build to continue.
        success_marker = clippy_success_marker,
        extra_clippy_flags = clippy_flags,
        error_format = error_format,
        clippy_diagnostics_file = clippy_diagnostics,
    )

    clippy_checks = [file for file in [clippy_out, clippy_success_marker] if file != None]
    output_group_info = {"clippy_checks": depset(clippy_checks)}
    if clippy_diagnostics:
        output_group_info["clippy_output"] = depset([clippy_diagnostics])

    return [
        OutputGroupInfo(**output_group_info),
        ClippyInfo(output = depset([clippy_out])),
    ]

# Example: Run the clippy checker on all targets in the codebase.
#   bazel build --aspects=@rules_rust//rust:defs.bzl%rust_clippy_aspect \
#               --output_groups=clippy_checks \
#               //...
rust_clippy_aspect = aspect(
    fragments = ["cpp"],
    attrs = {
        "_capture_output": attr.label(
            doc = "Value of the `capture_clippy_output` build setting",
            default = Label("//rust/settings:capture_clippy_output"),
        ),
        "_clippy_error_format": attr.label(
            doc = "The desired `--error-format` flags for clippy",
            default = "//rust/settings:clippy_error_format",
        ),
        "_clippy_flag": attr.label(
            doc = "Arguments to pass to clippy." +
                  "Multiple uses are accumulated and appended after the extra_rustc_flags.",
            default = Label("//rust/settings:clippy_flag"),
        ),
        "_clippy_flags": attr.label(
            doc = "Arguments to pass to clippy",
            default = Label("//rust/settings:clippy_flags"),
        ),
        "_clippy_output_diagnostics": attr.label(
            doc = "Value of the `clippy_output_diagnostics` build setting",
            default = "//rust/settings:clippy_output_diagnostics",
        ),
        "_config": attr.label(
            doc = "The `clippy.toml` file used for configuration",
            allow_single_file = True,
            default = Label("//rust/settings:clippy.toml"),
        ),
        "_error_format": attr.label(
            doc = "The desired `--error-format` flags for rustc",
            default = "//rust/settings:error_format",
        ),
        "_extra_rustc_flag": attr.label(
            default = Label("//rust/settings:extra_rustc_flag"),
        ),
        "_incompatible_change_clippy_error_format": attr.label(
            doc = "Whether to use the _clippy_error_format attribute",
            default = "//rust/settings:incompatible_change_clippy_error_format",
        ),
        "_per_crate_rustc_flag": attr.label(
            default = Label("//rust/settings:experimental_per_crate_rustc_flag"),
        ),
        "_process_wrapper": attr.label(
            doc = "A process wrapper for running clippy on all platforms",
            default = Label("//util/process_wrapper"),
            executable = True,
            cfg = "exec",
        ),
    },
    provides = [ClippyInfo],
    required_providers = [
        [rust_common.crate_info],
        [rust_common.test_crate_info],
    ],
    toolchains = [
        str(Label("//rust:toolchain_type")),
        config_common.toolchain_type("@bazel_tools//tools/cpp:toolchain_type", mandatory = False),
    ],
    implementation = _clippy_aspect_impl,
    doc = """\
Executes the clippy checker on specified targets.

This aspect applies to existing rust_library, rust_test, and rust_binary rules.

As an example, if the following is defined in `examples/hello_lib/BUILD.bazel`:

```python
load("@rules_rust//rust:defs.bzl", "rust_library", "rust_test")

rust_library(
    name = "hello_lib",
    srcs = ["src/lib.rs"],
)

rust_test(
    name = "greeting_test",
    srcs = ["tests/greeting.rs"],
    deps = [":hello_lib"],
)
```

Then the targets can be analyzed with clippy using the following command:

```output
$ bazel build --aspects=@rules_rust//rust:defs.bzl%rust_clippy_aspect \
              --output_groups=clippy_checks //hello_lib:all
```
""",
)

def _rust_clippy_rule_impl(ctx):
    clippy_ready_targets = [dep for dep in ctx.attr.deps if "clippy_checks" in dir(dep[OutputGroupInfo])]
    files = depset([], transitive = [dep[OutputGroupInfo].clippy_checks for dep in clippy_ready_targets])
    return [DefaultInfo(files = files)]

rust_clippy = rule(
    implementation = _rust_clippy_rule_impl,
    attrs = {
        "deps": attr.label_list(
            doc = "Rust targets to run clippy on.",
            providers = [
                [rust_common.crate_info],
                [rust_common.test_crate_info],
            ],
            aspects = [rust_clippy_aspect],
        ),
    },
    doc = """\
Executes the clippy checker on a specific target.

Similar to `rust_clippy_aspect`, but allows specifying a list of dependencies \
within the build system.

For example, given the following example targets:

```python
load("@rules_rust//rust:defs.bzl", "rust_library", "rust_test")

rust_library(
    name = "hello_lib",
    srcs = ["src/lib.rs"],
)

rust_test(
    name = "greeting_test",
    srcs = ["tests/greeting.rs"],
    deps = [":hello_lib"],
)
```

Rust clippy can be set as a build target with the following:

```python
load("@rules_rust//rust:defs.bzl", "rust_clippy")

rust_clippy(
    name = "hello_library_clippy",
    testonly = True,
    deps = [
        ":hello_lib",
        ":greeting_test",
    ],
)
```
""",
)

def _capture_clippy_output_impl(ctx):
    """Implementation of the `capture_clippy_output` rule

    Args:
        ctx (ctx): The rule's context object

    Returns:
        list: A list containing the CaptureClippyOutputInfo provider
    """
    return [CaptureClippyOutputInfo(capture_output = ctx.build_setting_value)]

capture_clippy_output = rule(
    doc = "Control whether to print clippy output or store it to a file, using the configured error_format.",
    implementation = _capture_clippy_output_impl,
    build_setting = config.bool(flag = True),
)

def _clippy_output_diagnostics_impl(ctx):
    """Implementation of the `clippy_output_diagnostics` rule

    Args:
        ctx (ctx): The rule's context object

    Returns:
        list: A list containing the CaptureClippyOutputInfo provider
    """
    return [ClippyOutputDiagnosticsInfo(output_diagnostics = ctx.build_setting_value)]

clippy_output_diagnostics = rule(
    doc = (
        "Setting this flag from the command line with `--@rules_rust//rust/settings:clippy_output_diagnostics` " +
        "makes rules_rust save lippy json output (suitable for consumption by rust-analyzer) in a file, " +
        "available from the `clippy_output` output group. This is the clippy equivalent of " +
        "`@rules_rust//settings:rustc_output_diagnostics`."
    ),
    implementation = _clippy_output_diagnostics_impl,
    build_setting = config.bool(flag = True),
)
