"""A module defining Rust 'unpretty' rules"""

load("//rust/private:common.bzl", "rust_common")
load(
    "//rust/private:rust.bzl",
    "RUSTC_ATTRS",
    "get_rust_test_flags",
)
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

# This list is determined by running the following command:
#
#   rustc +nightly -Zunpretty=
#
_UNPRETTY_MODES = [
    "ast-tree,expanded",
    "ast-tree",
    "expanded,hygiene",
    "expanded,identified",
    "expanded",
    "hir-tree",
    "hir,identified",
    "hir,typed",
    "hir",
    "identified",
    "mir-cfg",
    "mir",
    "normal",
]

RustUnprettyInfo = provider(
    doc = "A provider describing the Rust unpretty mode.",
    fields = {
        "modes": "Depset[string]: Can be any of {}".format(["'{}'".format(m) for m in _UNPRETTY_MODES]),
    },
)

def _rust_unpretty_flag_impl(ctx):
    value = ctx.build_setting_value
    invalid = []
    for mode in value:
        if mode not in _UNPRETTY_MODES:
            invalid.append(mode)
    if invalid:
        fail("{} build setting allowed to take values [{}] but was set to unallowed values: {}".format(
            ctx.label,
            ", ".join(["'{}'".format(m) for m in _UNPRETTY_MODES]),
            invalid,
        ))

    return RustUnprettyInfo(modes = depset(value))

rust_unpretty_flag = rule(
    doc = "A build setting which represents the Rust unpretty mode. The allowed values are {}".format(_UNPRETTY_MODES),
    implementation = _rust_unpretty_flag_impl,
    build_setting = config.string_list(
        flag = True,
        repeatable = True,
    ),
)

def _nightly_unpretty_transition_impl(settings, attr):
    mode = settings[str(Label("//rust/settings:unpretty"))]

    # Use the presence of _unpretty_modes as a proxy for whether this is a rust_unpretty target.
    if hasattr(attr, "_unpretty_modes") and hasattr(attr, "mode"):
        mode = mode + [attr.mode]

    return {
        str(Label("//rust/settings:unpretty")): depset(mode).to_list(),
        str(Label("//rust/toolchain/channel")): "nightly",
    }

nightly_unpretty_transition = transition(
    implementation = _nightly_unpretty_transition_impl,
    inputs = [str(Label("//rust/settings:unpretty"))],
    outputs = [
        str(Label("//rust/settings:unpretty")),
        str(Label("//rust/toolchain/channel")),
    ],
)

def _get_unpretty_ready_crate_info(target, aspect_ctx):
    """Check that a target is suitable for expansion and extract the `CrateInfo` provider from it.

    Args:
        target (Target): The target the aspect is running on.
        aspect_ctx (ctx, optional): The aspect's context object.

    Returns:
        CrateInfo, optional: A `CrateInfo` provider if rust unpretty should be run or `None`.
    """

    # Ignore external targets
    if target.label.workspace_root.startswith("external"):
        return None

    # Targets with specific tags will not be formatted
    if aspect_ctx:
        ignore_tags = [
            "nounpretty",
            "no-unpretty",
            "no_unpretty",
        ]

        for tag in ignore_tags:
            if tag in aspect_ctx.rule.attr.tags:
                return None

    # Obviously ignore any targets that don't contain `CrateInfo`
    if rust_common.crate_info not in target:
        return None

    return target[rust_common.crate_info]

def _rust_unpretty_aspect_impl(target, ctx):
    crate_info = _get_unpretty_ready_crate_info(target, ctx)
    if not crate_info:
        return []

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
        # Rust expand doesn't need to invoke transitive linking, therefore doesn't need linkstamps.
        depset([]),
        toolchain,
        cc_toolchain,
        feature_configuration,
        crate_info,
        dep_info,
        build_info,
    )

    output_groups = {}
    outputs = []

    for mode in ctx.attr._unpretty_modes[RustUnprettyInfo].modes.to_list():
        pretty_mode = mode.replace("-", "_")
        mnemonic = "RustUnpretty{}".format("".join([
            o.title()
            for m in pretty_mode.split(",")
            for o in m.split("_")
        ]))

        unpretty_out = ctx.actions.declare_file(
            "{}.unpretty.{}.rs".format(ctx.label.name, pretty_mode.replace(",", ".")),
            sibling = crate_info.output,
        )

        output_groups.update({"rust_unpretty_{}".format(pretty_mode.replace(",", "_")): depset([unpretty_out])})
        outputs.append(unpretty_out)

        rust_flags = []
        if crate_info.is_test:
            rust_flags = get_rust_test_flags(ctx.rule.attr)

        args, env = construct_arguments(
            ctx = ctx,
            attr = ctx.rule.attr,
            file = ctx.file,
            toolchain = toolchain,
            tool_path = toolchain.rustc.path,
            cc_toolchain = cc_toolchain,
            feature_configuration = feature_configuration,
            crate_info = crate_info,
            dep_info = dep_info,
            linkstamp_outs = linkstamp_outs,
            ambiguous_libs = ambiguous_libs,
            output_hash = determine_output_hash(crate_info.root, ctx.label),
            rust_flags = rust_flags,
            out_dir = out_dir,
            build_env_files = build_env_files,
            build_flags_files = build_flags_files,
            emit = ["dep-info", "metadata"],
            skip_expanding_rustc_env = True,
        )

        args.process_wrapper_flags.add("--stdout-file", unpretty_out)

        # Expand all macros and dump the source to stdout.
        # Tracking issue: https://github.com/rust-lang/rust/issues/43364
        args.rustc_flags.add("-Zunpretty={}".format(mode))

        ctx.actions.run(
            executable = ctx.executable._process_wrapper,
            inputs = compile_inputs,
            outputs = [unpretty_out],
            env = env,
            arguments = args.all,
            mnemonic = mnemonic,
            toolchain = "@rules_rust//rust:toolchain_type",
        )

    output_groups.update({"rust_unpretty": depset(outputs)})

    return [
        OutputGroupInfo(**output_groups),
    ]

# Example: Expand all rust targets in the codebase.
#   bazel build --aspects=@rules_rust//rust:defs.bzl%rust_unpretty_aspect \
#               --output_groups=expanded \
#               //...
rust_unpretty_aspect = aspect(
    implementation = _rust_unpretty_aspect_impl,
    fragments = ["cpp"],
    attrs = {
        "_unpretty_modes": attr.label(
            doc = "The values to pass to `--unpretty`",
            providers = [RustUnprettyInfo],
            default = Label("//rust/settings:unpretty"),
        ),
    } | RUSTC_ATTRS,
    toolchains = [
        str(Label("//rust:toolchain_type")),
        "@bazel_tools//tools/cpp:toolchain_type",
    ],
    required_providers = [rust_common.crate_info],
    doc = """\
Executes Rust expand on specified targets.

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

Then the targets can be expanded with the following command:

```output
$ bazel build --aspects=@rules_rust//rust:defs.bzl%rust_unpretty_aspect \
              --output_groups=rust_unpretty_expanded //hello_lib:all
```
""",
)

def _rust_unpretty_rule_impl(ctx):
    mode = ctx.attr.mode
    output_group = "rust_unpretty_{}".format(mode.replace(",", "_").replace("-", "_"))
    files = []
    for target in ctx.attr.deps:
        files.append(getattr(target[OutputGroupInfo], output_group))

    return [DefaultInfo(files = depset(transitive = files))]

rust_unpretty = rule(
    implementation = _rust_unpretty_rule_impl,
    cfg = nightly_unpretty_transition,
    attrs = {
        "deps": attr.label_list(
            doc = "Rust targets to run unpretty on.",
            providers = [rust_common.crate_info],
            aspects = [rust_unpretty_aspect],
        ),
        "mode": attr.string(
            doc = "The value to pass to `--unpretty`",
            values = _UNPRETTY_MODES,
            default = "expanded",
        ),
        "_allowlist_function_transition": attr.label(
            default = Label("//tools/allowlists/function_transition_allowlist"),
        ),
        "_unpretty_modes": attr.label(
            doc = "The values to pass to `--unpretty`",
            providers = [RustUnprettyInfo],
            default = Label("//rust/settings:unpretty"),
        ),
    },
    doc = """\
Executes rust unpretty on a specific target.

Similar to `rust_unpretty_aspect`, but allows specifying a list of dependencies \
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

Rust expand can be set as a build target with the following:

```python
load("@rules_rust//rust:defs.bzl", "rust_unpretty")

rust_unpretty(
    name = "hello_library_expand",
    testonly = True,
    deps = [
        ":hello_lib",
        ":greeting_test",
    ],
    mode = "expand",
)
```
""",
)
