"""Bazel test rules for [wasm-bindgen](https://crates.io/crates/wasm-bindgen)"""

load("@rules_rust//rust:defs.bzl", "rust_common")

# buildifier: disable=bzl-visibility
load("@rules_rust//rust/private:rust.bzl", "RUSTC_ATTRS", "get_rust_test_flags")

# buildifier: disable=bzl-visibility
load("@rules_rust//rust/private:rustc.bzl", "rustc_compile_action")

# buildifier: disable=bzl-visibility
load(
    "@rules_rust//rust/private:utils.bzl",
    "determine_output_hash",
    "expand_dict_value_locations",
    "find_toolchain",
    "generate_output_diagnostics",
    "get_import_macro_deps",
    "transform_deps",
    "transform_sources",
)
load("//:providers.bzl", "RustWasmBindgenInfo")
load("//private:transitions.bzl", "wasm_bindgen_transition")

WasmBindgenTestCrateInfo = provider(
    doc = "A provider encompassing the crate from a `rust_wasm_bindgen` target.",
    fields = {
        "crate": "The underlying `rust_wasm_bindgen.crate`.",
    },
)

def _wasm_bindgen_test_crate_aspect(target, ctx):
    if WasmBindgenTestCrateInfo in target:
        return []

    crate = ctx.rule.attr.wasm_file[0]
    if rust_common.test_crate_info in crate:
        crate_info = crate[rust_common.test_crate_info].crate
    elif rust_common.crate_info in crate:
        crate_info = crate[rust_common.crate_info]
    else:
        fail("Unable to determine crate info from {}".format(crate))

    return [WasmBindgenTestCrateInfo(
        crate = crate_info,
    )]

wasm_bindgen_test_crate_aspect = aspect(
    doc = "An aspect for accessing the underlying crate from a `rust_wasm_bindgen` target.",
    implementation = _wasm_bindgen_test_crate_aspect,
)

def _rlocationpath(file, workspace_name):
    if file.short_path.startswith("../"):
        return file.short_path[len("../"):]

    return "{}/{}".format(workspace_name, file.short_path)

def _rust_wasm_bindgen_test_impl(ctx):
    wb_toolchain = ctx.toolchains[Label("//:toolchain_type")]
    if not wb_toolchain.webdriver:
        fail("The currently registered wasm_bindgen_toolchain does not have a webdriver assigned. Tests are unavailable without one.")

    toolchain = find_toolchain(ctx)

    crate_type = "bin"
    deps = transform_deps(ctx.attr.deps + [wb_toolchain.wasm_bindgen_test])
    proc_macro_deps = transform_deps(ctx.attr.proc_macro_deps + get_import_macro_deps(ctx))

    # Target is building the crate in `test` config
    if WasmBindgenTestCrateInfo in ctx.attr.wasm:
        crate = ctx.attr.wasm[WasmBindgenTestCrateInfo].crate
    elif rust_common.test_crate_info in ctx.attr.wasm:
        crate = ctx.attr.wasm[rust_common.test_crate_info].crate
    elif rust_common.crate_info in ctx.attr.wasm:
        crate = ctx.attr.wasm[rust_common.test_crate_info].crate
    else:
        fail("Unable to locate CrateInfo for target: {}".format(ctx.attr.wasm))

    output_hash = determine_output_hash(crate.root, ctx.label)
    output = ctx.actions.declare_file(
        "test-%s/%s%s" % (
            output_hash,
            ctx.label.name,
            toolchain.binary_ext,
        ),
    )

    srcs = crate.srcs.to_list()
    compile_data = depset(ctx.files.compile_data, transitive = [crate.compile_data]).to_list()
    srcs, compile_data, _crate_root = transform_sources(ctx, srcs, compile_data, None)

    # Optionally join compile data
    if crate.compile_data:
        compile_data = depset(ctx.files.compile_data, transitive = [crate.compile_data])
    else:
        compile_data = depset(ctx.files.compile_data)
    if crate.compile_data_targets:
        compile_data_targets = depset(ctx.attr.compile_data, transitive = [crate.compile_data_targets])
    else:
        compile_data_targets = depset(ctx.attr.compile_data)
    rustc_env_files = ctx.files.rustc_env_files + crate.rustc_env_files

    # crate.rustc_env is already expanded upstream in rust_library rule implementation
    rustc_env = dict(crate.rustc_env)
    data_paths = depset(direct = getattr(ctx.attr, "data", [])).to_list()
    rustc_env.update(expand_dict_value_locations(
        ctx,
        ctx.attr.rustc_env,
        data_paths,
        {},
    ))
    aliases = dict(crate.aliases)
    aliases.update(ctx.attr.aliases)

    # Build the test binary using the dependency's srcs.
    crate_info_dict = dict(
        name = crate.name,
        type = crate_type,
        root = crate.root,
        srcs = depset(srcs, transitive = [crate.srcs]).to_list(),
        deps = depset(deps, transitive = [crate.deps]).to_list(),
        proc_macro_deps = depset(proc_macro_deps, transitive = [crate.proc_macro_deps]).to_list(),
        aliases = {},
        output = output,
        rustc_output = generate_output_diagnostics(ctx, output),
        edition = crate.edition,
        rustc_env = rustc_env,
        rustc_env_files = rustc_env_files,
        is_test = True,
        compile_data = compile_data,
        compile_data_targets = compile_data_targets,
        wrapped_crate_type = crate.type,
        owner = ctx.label,
    )

    crate_providers = rustc_compile_action(
        ctx = ctx,
        attr = ctx.attr,
        toolchain = toolchain,
        crate_info_dict = crate_info_dict,
        rust_flags = get_rust_test_flags(ctx.attr),
        skip_expanding_rustc_env = True,
    )
    data = getattr(ctx.attr, "data", [])

    env = expand_dict_value_locations(
        ctx,
        getattr(ctx.attr, "env", {}),
        data,
        {},
    )

    components = "{}/{}".format(ctx.label.workspace_root, ctx.label.package).split("/")
    env["CARGO_MANIFEST_DIR"] = "/".join([c for c in components if c])

    wrapper = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.symlink(
        output = wrapper,
        target_file = ctx.executable._wrapper,
        is_executable = True,
    )

    if wb_toolchain.browser:
        env["BROWSER"] = _rlocationpath(wb_toolchain.browser, ctx.workspace_name)

    env["BROWSER_TYPE"] = wb_toolchain.browser_type
    env["WEBDRIVER"] = _rlocationpath(wb_toolchain.webdriver, ctx.workspace_name)
    env["WEBDRIVER_ARGS"] = " ".join(wb_toolchain.webdriver_args)
    env["WEBDRIVER_JSON"] = _rlocationpath(wb_toolchain.webdriver_json, ctx.workspace_name)
    env["WASM_BINDGEN_TEST_RUNNER"] = _rlocationpath(wb_toolchain.wasm_bindgen_test_runner, ctx.workspace_name)

    # Force the use of a browser for now as there is no node integration.
    env["WASM_BINDGEN_USE_BROWSER"] = "1"

    providers = []

    for prov in crate_providers:
        if type(prov) == "DefaultInfo":
            files = prov.files.to_list()
            if len(files) != 1:
                fail("Unexpected number of output files for `{}`: {}".format(ctx.label, files))
            wasm_file = files[0]
            env["TEST_WASM_BINARY"] = _rlocationpath(files[0], ctx.workspace_name)
            providers.append(DefaultInfo(
                files = prov.files,
                runfiles = prov.default_runfiles.merge(ctx.runfiles(files = [wasm_file], transitive_files = wb_toolchain.all_test_files)),
                executable = wrapper,
            ))
        else:
            providers.append(prov)

    providers.append(testing.TestEnvironment(env))

    return providers

rust_wasm_bindgen_test = rule(
    doc = "Rules for running [wasm-bindgen tests](https://rustwasm.github.io/wasm-bindgen/wasm-bindgen-test/index.html).",
    implementation = _rust_wasm_bindgen_test_impl,
    cfg = wasm_bindgen_transition,
    attrs = {
        "aliases": attr.label_keyed_string_dict(
            doc = """\
            Remap crates to a new name or moniker for linkage to this target

            These are other `rust_library` targets and will be presented as the new name given.
        """,
        ),
        "compile_data": attr.label_list(
            doc = """\
            List of files used by this rule at compile time.

            This attribute can be used to specify any data files that are embedded into
            the library, such as via the
            [`include_str!`](https://doc.rust-lang.org/std/macro.include_str!.html)
            macro.
        """,
            allow_files = True,
        ),
        "crate_features": attr.string_list(
            doc = """\
            List of features to enable for this crate.

            Features are defined in the code using the `#[cfg(feature = "foo")]`
            configuration option. The features listed here will be passed to `rustc`
            with `--cfg feature="${feature_name}"` flags.
        """,
        ),
        "data": attr.label_list(
            doc = """\
            List of files used by this rule at compile time and runtime.

            If including data at compile time with include_str!() and similar,
            prefer `compile_data` over `data`, to prevent the data also being included
            in the runfiles.
        """,
            allow_files = True,
        ),
        "deps": attr.label_list(
            doc = """\
                List of other libraries to be linked to this library target.

                These can be either other `rust_library` targets or `cc_library` targets if
                linking a native library.
            """,
        ),
        "edition": attr.string(
            doc = "The rust edition to use for this crate. Defaults to the edition specified in the rust_toolchain.",
        ),
        "env": attr.string_dict(
            mandatory = False,
            doc = """\
            Specifies additional environment variables to set when the test is executed by bazel test.
            Values are subject to `$(rootpath)`, `$(execpath)`, location, and
            ["Make variable"](https://docs.bazel.build/versions/master/be/make-variables.html) substitution.
        """,
        ),
        "env_inherit": attr.string_list(
            doc = "Specifies additional environment variables to inherit from the external environment when the test is executed by bazel test.",
        ),
        "proc_macro_deps": attr.label_list(
            doc = """\
                List of `rust_proc_macro` targets used to help build this library target.
            """,
            cfg = "exec",
            providers = [rust_common.crate_info],
        ),
        "rustc_env": attr.string_dict(
            doc = """\
            Dictionary of additional `"key": "value"` environment variables to set for rustc.

            rust_test()/rust_binary() rules can use $(rootpath //package:target) to pass in the
            location of a generated file or external tool. Cargo build scripts that wish to
            expand locations should use cargo_build_script()'s build_script_env argument instead,
            as build scripts are run in a different environment - see cargo_build_script()'s
            documentation for more.
        """,
        ),
        "rustc_env_files": attr.label_list(
            doc = """\
            Files containing additional environment variables to set for rustc.

            These files should  contain a single variable per line, of format
            `NAME=value`, and newlines may be included in a value by ending a
            line with a trailing back-slash (`\\\\`).

            The order that these files will be processed is unspecified, so
            multiple definitions of a particular variable are discouraged.

            Note that the variables here are subject to
            [workspace status](https://docs.bazel.build/versions/main/user-manual.html#workspace_status)
            stamping should the `stamp` attribute be enabled. Stamp variables
            should be wrapped in brackets in order to be resolved. E.g.
            `NAME={WORKSPACE_STATUS_VARIABLE}`.
        """,
            allow_files = True,
        ),
        "rustc_flags": attr.string_list(
            doc = """\
            List of compiler flags passed to `rustc`.

            These strings are subject to Make variable expansion for predefined
            source/output path variables like `$location`, `$execpath`, and
            `$rootpath`. This expansion is useful if you wish to pass a generated
            file of arguments to rustc: `@$(location //package:target)`.
        """,
        ),
        "target_arch": attr.string(
            doc = "The target architecture to use for the wasm-bindgen command line option.",
            default = "wasm32",
            values = ["wasm32", "wasm64"],
        ),
        "version": attr.string(
            doc = "A version to inject in the cargo environment variable.",
            default = "0.0.0",
        ),
        "wasm": attr.label(
            doc = "The wasm target to test.",
            aspects = [wasm_bindgen_test_crate_aspect],
            providers = [RustWasmBindgenInfo],
            mandatory = True,
        ),
        "_wrapper": attr.label(
            doc = "The process wrapper for wasm-bindgen-test-runner.",
            cfg = "exec",
            executable = True,
            default = Label("//private:wasm_bindgen_test_wrapper"),
        ),
    } | RUSTC_ATTRS,
    fragments = ["cpp"],
    toolchains = [
        str(Label("//:toolchain_type")),
        "@rules_rust//rust:toolchain_type",
        config_common.toolchain_type("@bazel_tools//tools/cpp:toolchain_type", mandatory = False),
    ],
    test = True,
)
