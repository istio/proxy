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

load("//rust/private:common.bzl", "rust_common")
load("//rust/private:providers.bzl", "CaptureClippyOutputInfo", "ClippyInfo")
load(
    "//rust/private:rustc.bzl",
    "collect_deps",
    "collect_inputs",
    "construct_arguments",
)
load(
    "//rust/private:utils.bzl",
    "determine_output_hash",
    "find_cc_toolchain",
    "find_toolchain",
)

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

def _get_clippy_ready_crate_info(target, aspect_ctx = None):
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

def _clippy_aspect_impl(target, ctx):
    # Exit early if a target already has a clippy output group. This
    # can be useful for rules which always want to inhibit clippy.
    if OutputGroupInfo in target:
        if hasattr(target[OutputGroupInfo], "clippy_checks"):
            return []

    crate_info = _get_clippy_ready_crate_info(target, ctx)
    if not crate_info:
        return [ClippyInfo(output = depset([]))]

    toolchain = find_toolchain(ctx)
    cc_toolchain, feature_configuration = find_cc_toolchain(ctx)

    dep_info, build_info, _ = collect_deps(
        deps = crate_info.deps,
        proc_macro_deps = crate_info.proc_macro_deps,
        aliases = crate_info.aliases,
    )

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
    )

    args, env = construct_arguments(
        ctx = ctx,
        attr = ctx.rule.attr,
        file = ctx.file,
        toolchain = toolchain,
        tool_path = toolchain.clippy_driver.path,
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
    )

    if crate_info.is_test:
        args.rustc_flags.add("--test")

    clippy_flags = ctx.attr._clippy_flags[ClippyFlagsInfo].clippy_flags

    if hasattr(ctx.attr, "_clippy_flag"):
        clippy_flags = clippy_flags + ctx.attr._clippy_flag[ClippyFlagsInfo].clippy_flags

    # For remote execution purposes, the clippy_out file must be a sibling of crate_info.output
    # or rustc may fail to create intermediate output files because the directory does not exist.
    if ctx.attr._capture_output[CaptureClippyOutputInfo].capture_output:
        clippy_out = ctx.actions.declare_file(ctx.label.name + ".clippy.out", sibling = crate_info.output)
        args.process_wrapper_flags.add("--stderr-file", clippy_out)

        if clippy_flags:
            args.rustc_flags.add_all(clippy_flags)

        # If we are capturing the output, we want the build system to be able to keep going
        # and consume the output. Some clippy lints are denials, so we cap everything at warn.
        args.rustc_flags.add("--cap-lints=warn")
    else:
        # A marker file indicating clippy has executed successfully.
        # This file is necessary because "ctx.actions.run" mandates an output.
        clippy_out = ctx.actions.declare_file(ctx.label.name + ".clippy.ok", sibling = crate_info.output)
        args.process_wrapper_flags.add("--touch-file", clippy_out)

        if clippy_flags:
            args.rustc_flags.add_all(clippy_flags)
        else:
            # The user didn't provide any clippy flags explicitly so we apply conservative defaults.

            # Turn any warnings from clippy or rustc into an error, as otherwise
            # Bazel will consider the execution result of the aspect to be "success",
            # and Clippy won't be re-triggered unless the source file is modified.
            args.rustc_flags.add("-Dwarnings")

    # Upstream clippy requires one of these two filenames or it silently uses
    # the default config. Enforce the naming so users are not confused.
    valid_config_file_names = [".clippy.toml", "clippy.toml"]
    if ctx.file._config.basename not in valid_config_file_names:
        fail("The clippy config file must be named one of: {}".format(valid_config_file_names))
    env["CLIPPY_CONF_DIR"] = "${{pwd}}/{}".format(ctx.file._config.dirname)
    compile_inputs = depset([ctx.file._config], transitive = [compile_inputs])

    ctx.actions.run(
        executable = ctx.executable._process_wrapper,
        inputs = compile_inputs,
        outputs = [clippy_out],
        env = env,
        tools = [toolchain.clippy_driver],
        arguments = args.all,
        mnemonic = "Clippy",
        progress_message = "Clippy %{label}",
        toolchain = "@rules_rust//rust:toolchain_type",
    )

    return [
        OutputGroupInfo(clippy_checks = depset([clippy_out])),
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
        "_cc_toolchain": attr.label(
            doc = (
                "Required attribute to access the cc_toolchain. See [Accessing the C++ toolchain]" +
                "(https://docs.bazel.build/versions/master/integrating-with-rules-cc.html#accessing-the-c-toolchain)"
            ),
            default = Label("@bazel_tools//tools/cpp:current_cc_toolchain"),
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
        "_config": attr.label(
            doc = "The `clippy.toml` file used for configuration",
            allow_single_file = True,
            default = Label("//rust/settings:clippy.toml"),
        ),
        "_error_format": attr.label(
            doc = "The desired `--error-format` flags for clippy",
            default = "//rust/settings:error_format",
        ),
        "_extra_rustc_flag": attr.label(
            default = Label("//rust/settings:extra_rustc_flag"),
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
        "@bazel_tools//tools/cpp:toolchain_type",
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
