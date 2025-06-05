# Copyright 2018 The Bazel Authors. All rights reserved.
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

"""Rules for generating documentation with `rustdoc` for Bazel built crates"""

load("//rust/private:common.bzl", "rust_common")
load("//rust/private:rustc.bzl", "collect_deps", "collect_inputs", "construct_arguments")
load("//rust/private:utils.bzl", "dedent", "find_cc_toolchain", "find_toolchain")

def _strip_crate_info_output(crate_info):
    """Set the CrateInfo.output to None for a given CrateInfo provider.

    Args:
        crate_info (CrateInfo): A provider

    Returns:
        CrateInfo: A modified CrateInfo provider
    """
    return rust_common.create_crate_info(
        name = crate_info.name,
        type = crate_info.type,
        root = crate_info.root,
        srcs = crate_info.srcs,
        deps = crate_info.deps,
        proc_macro_deps = crate_info.proc_macro_deps,
        aliases = crate_info.aliases,
        # This crate info should have no output
        output = None,
        metadata = None,
        edition = crate_info.edition,
        rustc_env = crate_info.rustc_env,
        rustc_env_files = crate_info.rustc_env_files,
        is_test = crate_info.is_test,
        compile_data = crate_info.compile_data,
        compile_data_targets = crate_info.compile_data_targets,
        data = crate_info.data,
    )

def rustdoc_compile_action(
        ctx,
        toolchain,
        crate_info,
        output = None,
        rustdoc_flags = [],
        is_test = False):
    """Create a struct of information needed for a `rustdoc` compile action based on crate passed to the rustdoc rule.

    Args:
        ctx (ctx): The rule's context object.
        toolchain (rust_toolchain): The currently configured `rust_toolchain`.
        crate_info (CrateInfo): The provider of the crate passed to a rustdoc rule.
        output (File, optional): An optional output a `rustdoc` action is intended to produce.
        rustdoc_flags (list, optional): A list of `rustdoc` specific flags.
        is_test (bool, optional): If True, the action will be configured for `rust_doc_test` targets

    Returns:
        struct: A struct of some `ctx.actions.run` arguments.
    """

    # If an output was provided, ensure it's used in rustdoc arguments
    if output:
        rustdoc_flags = [
            "--output",
            output.path,
        ] + rustdoc_flags

    cc_toolchain, feature_configuration = find_cc_toolchain(ctx)

    dep_info, build_info, _ = collect_deps(
        deps = crate_info.deps,
        proc_macro_deps = crate_info.proc_macro_deps,
        aliases = crate_info.aliases,
    )

    compile_inputs, out_dir, build_env_files, build_flags_files, linkstamp_outs, ambiguous_libs = collect_inputs(
        ctx = ctx,
        file = ctx.file,
        files = ctx.files,
        linkstamps = depset([]),
        toolchain = toolchain,
        cc_toolchain = cc_toolchain,
        feature_configuration = feature_configuration,
        crate_info = crate_info,
        dep_info = dep_info,
        build_info = build_info,
        # If this is a rustdoc test, we need to depend on rlibs rather than .rmeta.
        force_depend_on_objects = is_test,
        include_link_flags = False,
    )

    # Since this crate is not actually producing the output described by the
    # given CrateInfo, this attribute needs to be stripped to allow the rest
    # of the rustc functionality in `construct_arguments` to avoid generating
    # arguments expecting to do so.
    rustdoc_crate_info = _strip_crate_info_output(crate_info)

    args, env = construct_arguments(
        ctx = ctx,
        attr = ctx.attr,
        file = ctx.file,
        toolchain = toolchain,
        tool_path = toolchain.rust_doc.short_path if is_test else toolchain.rust_doc.path,
        cc_toolchain = cc_toolchain,
        feature_configuration = feature_configuration,
        crate_info = rustdoc_crate_info,
        dep_info = dep_info,
        linkstamp_outs = linkstamp_outs,
        ambiguous_libs = ambiguous_libs,
        output_hash = None,
        rust_flags = rustdoc_flags,
        out_dir = out_dir,
        build_env_files = build_env_files,
        build_flags_files = build_flags_files,
        emit = [],
        remap_path_prefix = None,
        add_flags_for_binary = True,
        include_link_flags = False,
        force_depend_on_objects = is_test,
        skip_expanding_rustc_env = True,
    )

    # Because rustdoc tests compile tests outside of the sandbox, the sysroot
    # must be updated to the `short_path` equivilant as it will now be
    # a part of runfiles.
    if is_test:
        if "SYSROOT" in env:
            env.update({"SYSROOT": "${{pwd}}/{}".format(toolchain.sysroot_short_path)})
        if "OUT_DIR" in env:
            env.update({"OUT_DIR": "${{pwd}}/{}".format(build_info.out_dir.short_path)})

    return struct(
        executable = ctx.executable._process_wrapper,
        inputs = depset([crate_info.output], transitive = [compile_inputs]),
        env = env,
        arguments = args.all,
        tools = [toolchain.rust_doc],
    )

def _zip_action(ctx, input_dir, output_zip, crate_label):
    """Creates an archive of the generated documentation from `rustdoc`

    Args:
        ctx (ctx): The `rust_doc` rule's context object
        input_dir (File): A directory containing the outputs from rustdoc
        output_zip (File): The location of the output archive containing generated documentation
        crate_label (Label): The label of the crate docs are being generated for.
    """
    args = ctx.actions.args()
    args.add(ctx.executable._zipper)
    args.add(output_zip)
    args.add(ctx.bin_dir.path)
    args.add_all([input_dir], expand_directories = True)
    ctx.actions.run(
        executable = ctx.executable._dir_zipper,
        inputs = [input_dir],
        outputs = [output_zip],
        arguments = [args],
        mnemonic = "RustdocZip",
        progress_message = "Creating RustdocZip for {}".format(crate_label),
        tools = [ctx.executable._zipper],
    )

def _rust_doc_impl(ctx):
    """The implementation of the `rust_doc` rule

    Args:
        ctx (ctx): The rule's context object
    """

    if ctx.attr.rustc_flags:
        # buildifier: disable=print
        print("rustc_flags is deprecated in favor of `rustdoc_flags` for rustdoc targets. Please update {}".format(
            ctx.label,
        ))

    crate = ctx.attr.crate
    crate_info = crate[rust_common.crate_info]

    output_dir = ctx.actions.declare_directory("{}.rustdoc".format(ctx.label.name))

    # Add the current crate as an extern for the compile action
    rustdoc_flags = [
        "--extern",
        "{}={}".format(crate_info.name, crate_info.output.path),
    ]

    rustdoc_flags.extend(ctx.attr.rustdoc_flags)

    action = rustdoc_compile_action(
        ctx = ctx,
        toolchain = find_toolchain(ctx),
        crate_info = crate_info,
        output = output_dir,
        rustdoc_flags = rustdoc_flags,
    )

    ctx.actions.run(
        mnemonic = "Rustdoc",
        progress_message = "Generating Rustdoc for {}".format(crate.label),
        outputs = [output_dir],
        executable = action.executable,
        inputs = action.inputs,
        env = action.env,
        arguments = action.arguments,
        tools = action.tools,
    )

    # This rule does nothing without a single-file output, though the directory should've sufficed.
    _zip_action(ctx, output_dir, ctx.outputs.rust_doc_zip, crate.label)

    return [
        DefaultInfo(
            files = depset([output_dir]),
        ),
        OutputGroupInfo(
            rustdoc_dir = depset([output_dir]),
            rustdoc_zip = depset([ctx.outputs.rust_doc_zip]),
        ),
    ]

rust_doc = rule(
    doc = dedent("""\
    Generates code documentation.

    Example:
    Suppose you have the following directory structure for a Rust library crate:

    ```
    [workspace]/
        WORKSPACE
        hello_lib/
            BUILD
            src/
                lib.rs
    ```

    To build [`rustdoc`][rustdoc] documentation for the `hello_lib` crate, define \
    a `rust_doc` rule that depends on the the `hello_lib` `rust_library` target:

    [rustdoc]: https://doc.rust-lang.org/book/documentation.html

    ```python
    package(default_visibility = ["//visibility:public"])

    load("@rules_rust//rust:defs.bzl", "rust_library", "rust_doc")

    rust_library(
        name = "hello_lib",
        srcs = ["src/lib.rs"],
    )

    rust_doc(
        name = "hello_lib_doc",
        crate = ":hello_lib",
    )
    ```

    Running `bazel build //hello_lib:hello_lib_doc` will build a zip file containing \
    the documentation for the `hello_lib` library crate generated by `rustdoc`.
    """),
    implementation = _rust_doc_impl,
    attrs = {
        "crate": attr.label(
            doc = (
                "The label of the target to generate code documentation for.\n" +
                "\n" +
                "`rust_doc` can generate HTML code documentation for the source files of " +
                "`rust_library` or `rust_binary` targets."
            ),
            providers = [rust_common.crate_info],
            mandatory = True,
        ),
        "html_after_content": attr.label(
            doc = "File to add in `<body>`, after content.",
            allow_single_file = [".html", ".md"],
        ),
        "html_before_content": attr.label(
            doc = "File to add in `<body>`, before content.",
            allow_single_file = [".html", ".md"],
        ),
        "html_in_header": attr.label(
            doc = "File to add to `<head>`.",
            allow_single_file = [".html", ".md"],
        ),
        "markdown_css": attr.label_list(
            doc = "CSS files to include via `<link>` in a rendered Markdown file.",
            allow_files = [".css"],
        ),
        "rustc_flags": attr.string_list(
            doc = "**Deprecated**: use `rustdoc_flags` instead",
        ),
        "rustdoc_flags": attr.string_list(
            doc = dedent("""\
                List of flags passed to `rustdoc`.

                These strings are subject to Make variable expansion for predefined
                source/output path variables like `$location`, `$execpath`, and
                `$rootpath`. This expansion is useful if you wish to pass a generated
                file of arguments to rustc: `@$(location //package:target)`.
            """),
        ),
        "_cc_toolchain": attr.label(
            doc = "In order to use find_cpp_toolchain, you must define the '_cc_toolchain' attribute on your rule or aspect.",
            default = Label("@bazel_tools//tools/cpp:current_cc_toolchain"),
        ),
        "_dir_zipper": attr.label(
            doc = "A tool that orchestrates the creation of zip archives for rustdoc outputs.",
            default = Label("//util/dir_zipper"),
            cfg = "exec",
            executable = True,
        ),
        "_error_format": attr.label(
            default = Label("//rust/settings:error_format"),
        ),
        "_process_wrapper": attr.label(
            doc = "A process wrapper for running rustdoc on all platforms",
            default = Label("@rules_rust//util/process_wrapper"),
            executable = True,
            allow_single_file = True,
            cfg = "exec",
        ),
        "_zipper": attr.label(
            doc = "A Bazel provided tool for creating archives",
            default = Label("@bazel_tools//tools/zip:zipper"),
            cfg = "exec",
            executable = True,
        ),
    },
    fragments = ["cpp"],
    outputs = {
        "rust_doc_zip": "%{name}.zip",
    },
    toolchains = [
        str(Label("//rust:toolchain_type")),
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
)
