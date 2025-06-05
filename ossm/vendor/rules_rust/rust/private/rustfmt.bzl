"""A module defining rustfmt rules"""

load(":common.bzl", "rust_common")

def _get_rustfmt_ready_crate_info(target):
    """Check that a target is suitable for rustfmt and extract the `CrateInfo` provider from it.

    Args:
        target (Target): The target the aspect is running on.

    Returns:
        CrateInfo, optional: A `CrateInfo` provider if clippy should be run or `None`.
    """

    # Ignore external targets
    if target.label.workspace_name:
        return None

    # Obviously ignore any targets that don't contain `CrateInfo`
    if rust_common.crate_info in target:
        return target[rust_common.crate_info]
    elif rust_common.test_crate_info in target:
        return target[rust_common.test_crate_info].crate
    else:
        return None

def _find_rustfmtable_srcs(crate_info, aspect_ctx = None):
    """Parse a `CrateInfo` provider for rustfmt formattable sources.

    Args:
        crate_info (CrateInfo): A `CrateInfo` provider.
        aspect_ctx (ctx, optional): The aspect's context object.

    Returns:
        list: A list of formattable sources (`File`).
    """

    # Targets with specific tags will not be formatted
    if aspect_ctx:
        ignore_tags = [
            "no_format",
            "no_rustfmt",
            "norustfmt",
        ]

        for tag in aspect_ctx.rule.attr.tags:
            if tag.replace("-", "_").lower() in ignore_tags:
                return []

    # Filter out any generated files
    srcs = [src for src in crate_info.srcs.to_list() if src.is_source]

    return srcs

def _generate_manifest(edition, srcs, ctx):
    workspace = ctx.label.workspace_name or ctx.workspace_name

    # Gather the source paths to non-generated files
    content = ctx.actions.args()
    content.set_param_file_format("multiline")
    content.add_all(srcs, format_each = workspace + "/%s")
    content.add(edition)

    # Write the rustfmt manifest
    manifest = ctx.actions.declare_file(ctx.label.name + ".rustfmt")
    ctx.actions.write(
        output = manifest,
        content = content,
    )

    return manifest

def _perform_check(edition, srcs, ctx):
    rustfmt_toolchain = ctx.toolchains[Label("//rust/rustfmt:toolchain_type")]

    config = ctx.file._config
    marker = ctx.actions.declare_file(ctx.label.name + ".rustfmt.ok")

    args = ctx.actions.args()
    args.add("--touch-file", marker)
    args.add("--")
    args.add(rustfmt_toolchain.rustfmt)
    args.add("--config-path", config)
    args.add("--edition", edition)
    args.add("--check")
    args.add_all(srcs)

    ctx.actions.run(
        executable = ctx.executable._process_wrapper,
        inputs = srcs + [config],
        outputs = [marker],
        tools = [rustfmt_toolchain.all_files],
        arguments = [args],
        mnemonic = "Rustfmt",
        progress_message = "Rustfmt %{label}",
    )

    return marker

def _rustfmt_aspect_impl(target, ctx):
    # Exit early if a target already has a rustfmt output group. This
    # can be useful for rules which always want to inhibit rustfmt.
    if OutputGroupInfo in target:
        if hasattr(target[OutputGroupInfo], "rustfmt_checks"):
            return []

    crate_info = _get_rustfmt_ready_crate_info(target)

    if not crate_info:
        return []

    srcs = _find_rustfmtable_srcs(crate_info, ctx)

    # If there are no formattable sources, do nothing.
    if not srcs:
        return []

    edition = crate_info.edition

    marker = _perform_check(edition, srcs, ctx)

    return [
        OutputGroupInfo(
            rustfmt_checks = depset([marker]),
        ),
    ]

rustfmt_aspect = aspect(
    implementation = _rustfmt_aspect_impl,
    doc = """\
This aspect is used to gather information about a crate for use in rustfmt and perform rustfmt checks

Output Groups:

- `rustfmt_checks`: Executes `rustfmt --check` on the specified target.

The build setting `@rules_rust//rust/settings:rustfmt.toml` is used to control the Rustfmt [configuration settings][cs]
used at runtime.

[cs]: https://rust-lang.github.io/rustfmt/

This aspect is executed on any target which provides the `CrateInfo` provider. However
users may tag a target with `no-rustfmt` or `no-format` to have it skipped. Additionally,
generated source files are also ignored by this aspect.
""",
    attrs = {
        "_config": attr.label(
            doc = "The `rustfmt.toml` file used for formatting",
            allow_single_file = True,
            default = Label("//rust/settings:rustfmt.toml"),
        ),
        "_process_wrapper": attr.label(
            doc = "A process wrapper for running rustfmt on all platforms",
            cfg = "exec",
            executable = True,
            default = Label("//util/process_wrapper"),
        ),
    },
    required_providers = [
        [rust_common.crate_info],
        [rust_common.test_crate_info],
    ],
    fragments = ["cpp"],
    toolchains = [
        str(Label("//rust/rustfmt:toolchain_type")),
    ],
)

def _rustfmt_test_manifest_aspect_impl(target, ctx):
    crate_info = _get_rustfmt_ready_crate_info(target)

    if not crate_info:
        return []

    # Parse the edition to use for formatting from the target
    edition = crate_info.edition

    srcs = _find_rustfmtable_srcs(crate_info, ctx)
    manifest = _generate_manifest(edition, srcs, ctx)

    return [
        OutputGroupInfo(
            rustfmt_manifest = depset([manifest]),
        ),
    ]

# This aspect contains functionality split out of `rustfmt_aspect` which broke when
# `required_providers` was added to it. Aspects which have `required_providers` seems
# to not function with attributes that also require providers.
_rustfmt_test_manifest_aspect = aspect(
    implementation = _rustfmt_test_manifest_aspect_impl,
    doc = """\
This aspect is used to gather information about a crate for use in `rustfmt_test`

Output Groups:

- `rustfmt_manifest`: A manifest used by rustfmt binaries to provide crate specific settings.
""",
    fragments = ["cpp"],
    toolchains = [
        str(Label("//rust/rustfmt:toolchain_type")),
    ],
)

def _rustfmt_test_impl(ctx):
    # The executable of a test target must be the output of an action in
    # the rule implementation. This file is simply a symlink to the real
    # rustfmt test runner.
    is_windows = ctx.executable._runner.extension == ".exe"
    runner = ctx.actions.declare_file("{}{}".format(
        ctx.label.name,
        ".exe" if is_windows else "",
    ))

    ctx.actions.symlink(
        output = runner,
        target_file = ctx.executable._runner,
        is_executable = True,
    )

    crate_infos = [_get_rustfmt_ready_crate_info(target) for target in ctx.attr.targets]
    srcs = [depset(_find_rustfmtable_srcs(crate_info)) for crate_info in crate_infos if crate_info]

    # Some targets may be included in tests but tagged as "no-format". In this
    # case, there will be no manifest.
    manifests = [getattr(target[OutputGroupInfo], "rustfmt_manifest", None) for target in ctx.attr.targets]
    manifests = depset(transitive = [manifest for manifest in manifests if manifest])

    runfiles = ctx.runfiles(
        transitive_files = depset(transitive = srcs + [manifests]),
    )

    runfiles = runfiles.merge(
        ctx.attr._runner[DefaultInfo].default_runfiles,
    )

    workspace = ctx.label.workspace_name or ctx.workspace_name

    return [
        DefaultInfo(
            files = depset([runner]),
            runfiles = runfiles,
            executable = runner,
        ),
        RunEnvironmentInfo(
            environment = {
                "RUSTFMT_MANIFESTS": ctx.configuration.host_path_separator.join([
                    workspace + "/" + manifest.short_path
                    for manifest in sorted(manifests.to_list())
                ]),
                "RUST_BACKTRACE": "1",
            },
        ),
    ]

rustfmt_test = rule(
    implementation = _rustfmt_test_impl,
    doc = "A test rule for performing `rustfmt --check` on a set of targets",
    attrs = {
        "targets": attr.label_list(
            doc = "Rust targets to run `rustfmt --check` on.",
            providers = [
                [rust_common.crate_info],
                [rust_common.test_crate_info],
            ],
            aspects = [_rustfmt_test_manifest_aspect],
        ),
        "_runner": attr.label(
            doc = "The rustfmt test runner",
            cfg = "exec",
            executable = True,
            default = Label("//tools/rustfmt:rustfmt_test"),
        ),
    },
    test = True,
)

def _rustfmt_toolchain_impl(ctx):
    make_variables = {
        "RUSTFMT": ctx.file.rustfmt.path,
    }

    if ctx.attr.rustc:
        make_variables.update({
            "RUSTC": ctx.file.rustc.path,
        })

    make_variable_info = platform_common.TemplateVariableInfo(make_variables)

    all_files = [ctx.file.rustfmt] + ctx.files.rustc_lib
    if ctx.file.rustc:
        all_files.append(ctx.file.rustc)

    toolchain = platform_common.ToolchainInfo(
        rustfmt = ctx.file.rustfmt,
        rustc = ctx.file.rustc,
        rustc_lib = depset(ctx.files.rustc_lib),
        all_files = depset(all_files),
        make_variables = make_variable_info,
    )

    return [
        toolchain,
        make_variable_info,
    ]

rustfmt_toolchain = rule(
    doc = "A toolchain for [rustfmt](https://rust-lang.github.io/rustfmt/)",
    implementation = _rustfmt_toolchain_impl,
    attrs = {
        "rustc": attr.label(
            doc = "The location of the `rustc` binary. Can be a direct source or a filegroup containing one item.",
            allow_single_file = True,
            cfg = "exec",
        ),
        "rustc_lib": attr.label(
            doc = "The libraries used by rustc during compilation.",
            cfg = "exec",
        ),
        "rustfmt": attr.label(
            doc = "The location of the `rustfmt` binary. Can be a direct source or a filegroup containing one item.",
            allow_single_file = True,
            cfg = "exec",
            mandatory = True,
        ),
    },
    toolchains = [
        str(Label("@rules_rust//rust:toolchain_type")),
    ],
)

def _current_rustfmt_toolchain_impl(ctx):
    toolchain = ctx.toolchains[str(Label("@rules_rust//rust/rustfmt:toolchain_type"))]

    return [
        toolchain,
        toolchain.make_variables,
        DefaultInfo(
            files = depset([
                toolchain.rustfmt,
            ]),
            runfiles = ctx.runfiles(transitive_files = toolchain.all_files),
        ),
    ]

current_rustfmt_toolchain = rule(
    doc = "A rule for exposing the current registered `rustfmt_toolchain`.",
    implementation = _current_rustfmt_toolchain_impl,
    toolchains = [
        str(Label("@rules_rust//rust/rustfmt:toolchain_type")),
    ],
)
