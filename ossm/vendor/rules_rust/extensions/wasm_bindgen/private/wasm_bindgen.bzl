"""Bazel rules for [wasm-bindgen](https://crates.io/crates/wasm-bindgen)"""

load("@rules_rust//rust:defs.bzl", "rust_analyzer_aspect", "rust_clippy_aspect", "rust_common", "rustfmt_aspect")

# buildifier: disable=bzl-visibility
load("@rules_rust//rust/private:providers.bzl", "ClippyInfo", "RustAnalyzerGroupInfo", "RustAnalyzerInfo")
load("//:providers.bzl", "RustWasmBindgenInfo")
load(":transitions.bzl", "wasm_bindgen_transition")

def rust_wasm_bindgen_action(*, ctx, toolchain, wasm_file, target_output, flags = []):
    """Spawn a `RustWasmBindgen` action.

    Args:
        ctx (ctx): The rule's context object.
        toolchain (ToolchainInfo): The current `rust_wasm_bindgen_toolchain`.
        wasm_file (Target): The target representing the `.wasm` file.
        target_output (str): _description_
        flags (list, optional): Flags to pass to `wasm-bindgen`.

    Returns:
        RustWasmBindgenInfo: A provider containing action outputs.
    """
    bindgen_bin = toolchain.bindgen

    # Since the `wasm_file` attribute is behind a transition, it will be converted
    # to a list.
    if len(wasm_file) == 1:
        if rust_common.crate_info in wasm_file[0]:
            target = wasm_file[0]
            crate_info = target[rust_common.crate_info]

            # Provide a helpful warning informing users how to use the rule
            if rust_common.crate_info in target:
                supported_types = ["cdylib", "bin"]
                if crate_info.type not in supported_types:
                    fail("The target '{}' is not a supported type: {}".format(
                        ctx.attr.crate.label,
                        supported_types,
                    ))

            progress_message_label = target.label
            input_file = crate_info.output
        else:
            wasm_files = wasm_file[0][DefaultInfo].files.to_list()
            if len(wasm_files) != 1:
                fail("Unexpected number of wasm files: {}".format(wasm_files))

            progress_message_label = wasm_files[0].path
            input_file = wasm_files[0]
    else:
        fail("wasm_file is expected to be a transitioned label attr on `{}`. Got `{}`".format(
            ctx.label,
            wasm_file,
        ))

    out_name = ctx.label.name
    if ctx.attr.out_name:
        out_name = ctx.attr.out_name

    bindgen_wasm_module = ctx.actions.declare_file("{}/{}_bg.wasm".format(ctx.label.name, out_name))
    snippets = ctx.actions.declare_directory("{}/snippets".format(ctx.label.name))

    js_out = [ctx.actions.declare_file("{}/{}.js".format(ctx.label.name, out_name))]
    ts_out = []
    if not "--no-typescript" in flags:
        ts_out.append(ctx.actions.declare_file("{}/{}.d.ts".format(ctx.label.name, out_name)))

    if target_output == "bundler":
        js_out.append(ctx.actions.declare_file("{}/{}_bg.js".format(ctx.label.name, out_name)))
        if not "--no-typescript" in flags:
            ts_out.append(ctx.actions.declare_file("{}/{}_bg.wasm.d.ts".format(ctx.label.name, out_name)))

    elif target_output == "web":
        if not "--no-typescript" in flags:
            ts_out.append(ctx.actions.declare_file("{}/{}_bg.wasm.d.ts".format(ctx.label.name, out_name)))

    outputs = [bindgen_wasm_module, snippets] + js_out + ts_out

    args = ctx.actions.args()
    args.add("--target", target_output)
    args.add("--out-dir", bindgen_wasm_module.dirname)
    args.add("--out-name", out_name)
    args.add_all(flags)
    args.add(input_file)

    ctx.actions.run(
        executable = bindgen_bin,
        inputs = [input_file],
        outputs = outputs,
        mnemonic = "RustWasmBindgen",
        progress_message = "Generating WebAssembly bindings for {}".format(progress_message_label),
        arguments = [args],
        toolchain = str(Label("//:toolchain_type")),
    )

    return RustWasmBindgenInfo(
        wasm = bindgen_wasm_module,
        js = depset(js_out),
        ts = depset(ts_out),
        snippets = snippets,
        root = bindgen_wasm_module.dirname,
    )

def _rust_wasm_bindgen_impl(ctx):
    toolchain = ctx.toolchains[Label("//:toolchain_type")]

    info = rust_wasm_bindgen_action(
        ctx = ctx,
        toolchain = toolchain,
        wasm_file = ctx.attr.wasm_file,
        target_output = ctx.attr.target,
        flags = ctx.attr.bindgen_flags,
    )

    providers = [
        DefaultInfo(
            files = depset(
                [info.wasm, info.snippets],
                transitive = [info.js, info.ts],
            ),
        ),
        info,
    ]

    crate = ctx.attr.wasm_file[0]
    if RustAnalyzerGroupInfo in crate:
        providers.append(crate[RustAnalyzerGroupInfo])

    if RustAnalyzerInfo in crate:
        providers.append(crate[RustAnalyzerInfo])

    if ClippyInfo in crate:
        providers.append(crate[ClippyInfo])

    if OutputGroupInfo in crate:
        output_info = crate[OutputGroupInfo]
        output_groups = {}
        for group in ["rusfmt_checks", "clippy_checks", "rust_analyzer_crate_spec"]:
            if hasattr(output_info, group):
                output_groups[group] = getattr(output_info, group)

        providers.append(OutputGroupInfo(**output_groups))

    return providers

WASM_BINDGEN_ATTR = {
    "bindgen_flags": attr.string_list(
        doc = "Flags to pass directly to the wasm-bindgen executable. See https://github.com/rustwasm/wasm-bindgen/ for details.",
    ),
    "out_name": attr.string(
        doc = "Set a custom output filename (Without extension. Defaults to target name).",
    ),
    "target": attr.string(
        doc = "The type of output to generate. See https://rustwasm.github.io/wasm-bindgen/reference/deployment.html for details.",
        default = "bundler",
        values = ["web", "bundler", "nodejs", "no-modules", "deno"],
    ),
    "target_arch": attr.string(
        doc = "The target architecture to use for the wasm-bindgen command line option.",
        default = "wasm32",
        values = ["wasm32", "wasm64"],
    ),
    "wasm_file": attr.label(
        doc = "The `.wasm` crate to generate bindings for.",
        allow_single_file = True,
        providers = [
            [rust_common.test_crate_info],
            [rust_common.crate_info],
        ],
        aspects = [
            rust_analyzer_aspect,
            rustfmt_aspect,
            rust_clippy_aspect,
        ],
        cfg = wasm_bindgen_transition,
        mandatory = True,
    ),
    "_allowlist_function_transition": attr.label(
        default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
    ),
}

rust_wasm_bindgen = rule(
    implementation = _rust_wasm_bindgen_impl,
    doc = """\
Generates javascript and typescript bindings for a webassembly module using [wasm-bindgen][ws].

[ws]: https://rustwasm.github.io/docs/wasm-bindgen/

An example of this rule in use can be seen at [@rules_rust//examples/wasm](../examples/wasm)
""",
    attrs = WASM_BINDGEN_ATTR,
    toolchains = [
        str(Label("//:toolchain_type")),
    ],
)

def _rust_wasm_bindgen_toolchain_impl(ctx):
    all_test_files = depset()
    if ctx.attr.wasm_bindgen_test or ctx.attr.webdriver or ctx.attr.browser_type:
        if not ctx.attr.wasm_bindgen_test:
            fail("Not all webdriver attributes provided. Missing `wasm_bindgen_test` on `{}`".format(ctx.label))
        if not ctx.attr.webdriver:
            fail("Not all webdriver attributes provided. Missing `webdriver` on `{}`".format(ctx.label))
        if not ctx.attr.browser_type:
            fail("Not all webdriver attributes provided. Missing `browser_type` on `{}`".format(ctx.label))

        all_depsets = [
            ctx.attr.webdriver[DefaultInfo].files,
            ctx.attr.webdriver[DefaultInfo].default_runfiles.files,
            ctx.attr.wasm_bindgen_test_runner[DefaultInfo].files,
            ctx.attr.wasm_bindgen_test_runner[DefaultInfo].default_runfiles.files,
            ctx.attr.webdriver_json[DefaultInfo].files,
            ctx.attr.webdriver_json[DefaultInfo].default_runfiles.files,
        ]

        if ctx.attr.browser:
            all_depsets.extend([
                ctx.attr.browser[DefaultInfo].files,
                ctx.attr.browser[DefaultInfo].default_runfiles.files,
            ])

        all_test_files = depset(transitive = all_depsets)

    return platform_common.ToolchainInfo(
        wasm_bindgen_cli = ctx.executable.wasm_bindgen_cli,
        wasm_bindgen_test = ctx.attr.wasm_bindgen_test,
        wasm_bindgen_test_runner = ctx.executable.wasm_bindgen_test_runner,
        webdriver = ctx.executable.webdriver,
        webdriver_args = ctx.attr.webdriver_args,
        webdriver_json = ctx.file.webdriver_json,
        browser_type = ctx.attr.browser_type,
        browser = ctx.executable.browser,
        all_test_files = all_test_files,

        # Deprecated
        bindgen = ctx.executable.wasm_bindgen_cli,
    )

rust_wasm_bindgen_toolchain = rule(
    implementation = _rust_wasm_bindgen_toolchain_impl,
    doc = """\
The tools required for the `rust_wasm_bindgen` rule.

In cases where users want to control or change the version of `wasm-bindgen` used by [rust_wasm_bindgen](#rust_wasm_bindgen),
a unique toolchain can be created as in the example below:

```python
load("@rules_rust_wasm_bindgen//:defs.bzl", "rust_wasm_bindgen_toolchain")

rust_wasm_bindgen_toolchain(
    wasm_bindgen_cli = "//3rdparty/crates:wasm_bindgen_cli__bin",
)

toolchain(
    name = "wasm_bindgen_toolchain",
    toolchain = "wasm_bindgen_toolchain_impl",
    toolchain_type = "@rules_rust_wasm_bindgen//:toolchain_type",
)
```

Now that you have your own toolchain, you need to register it by
inserting the following statement in your `WORKSPACE` file:

```python
register_toolchains("//my/toolchains:wasm_bindgen_toolchain")
```

For additional information, see the [Bazel toolchains documentation][toolchains].

[toolchains]: https://docs.bazel.build/versions/master/toolchains.html
""",
    attrs = {
        "browser": attr.label(
            doc = "The browser entrypoint.",
            cfg = "exec",
            executable = True,
            allow_files = True,
        ),
        "browser_type": attr.string(
            doc = "The type of browser provided.",
            values = [
                "firefox",
                "chrome",
                "safari",
            ],
        ),
        "wasm_bindgen_cli": attr.label(
            doc = "The label of a `wasm-bindgen-cli` executable.",
            executable = True,
            cfg = "exec",
        ),
        "wasm_bindgen_test": attr.label(
            doc = "The label of a `wasm-bindgen-test` crate.",
            cfg = "target",
        ),
        "wasm_bindgen_test_runner": attr.label(
            doc = "The label of a `wasm-bindgen-test-runner` binary.",
            executable = True,
            cfg = "exec",
        ),
        "webdriver": attr.label(
            doc = "The webdriver to use.",
            executable = True,
            allow_files = True,
            cfg = "exec",
        ),
        "webdriver_args": attr.string_list(
            doc = "Arguments to pass to the `webdriver` binary.",
        ),
        "webdriver_json": attr.label(
            doc = "The [`webdriver.json` config file](https://rustwasm.github.io/wasm-bindgen/wasm-bindgen-test/browsers.html#configuring-headless-browser-capabilities) for wasm-bindgen-test.",
            allow_single_file = [".json"],
            cfg = "exec",
        ),
    },
)
