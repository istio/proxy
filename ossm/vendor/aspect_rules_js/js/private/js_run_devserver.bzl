"Implementation details for js_run_devserver rule"

load(":js_binary.bzl", "js_binary_lib")
load(":js_helpers.bzl", _gather_files_from_js_providers = "gather_files_from_js_providers")
load("@bazel_skylib//lib:dicts.bzl", "dicts")

_attrs = dicts.add(js_binary_lib.attrs, {
    "tool_exec_cfg": attr.label(
        executable = True,
        cfg = "exec",
    ),
    "tool_target_cfg": attr.label(
        executable = True,
        cfg = "target",
    ),
    "use_execroot_entry_point": attr.bool(
        default = True,
    ),
    "grant_sandbox_write_permissions": attr.bool(),
    "allow_execroot_entry_point_with_no_copy_data_to_bin": attr.bool(),
    "command": attr.string(),
})

def _js_run_devserver_impl(ctx):
    config_file = ctx.actions.declare_file("{}_config.json".format(ctx.label.name))

    launcher = js_binary_lib.create_launcher(
        ctx,
        log_prefix_rule_set = "aspect_rules_js",
        log_prefix_rule = "js_run_devserver",
        fixed_args = [config_file.short_path],
    )

    use_tool = ctx.attr.tool_target_cfg or ctx.attr.tool_exec_cfg
    if use_tool and (not ctx.attr.tool_exec_cfg or not ctx.attr.tool_target_cfg):
        fail("Internal error")

    if not use_tool and not ctx.attr.command:
        fail("Either tool or command must be specified")
    if use_tool and ctx.attr.command:
        fail("Only one of tool or command may be specified")

    transitive_runfiles = [_gather_files_from_js_providers(
        targets = ctx.attr.data,
        include_transitive_sources = ctx.attr.include_transitive_sources,
        include_declarations = ctx.attr.include_declarations,
        include_npm_linked_packages = ctx.attr.include_npm_linked_packages,
    )]

    # The .to_list() calls here are intentional and cannot be avoided; they should be small sets of
    # files as they only include direct npm links (node_modules/foo) and the virtual store tree
    # artifacts those symlinks point to (node_modules/.aspect_rules_js/foo@1.2.3/node_modules/foo)
    data_files = []
    for f in depset(transitive = transitive_runfiles + [dep.files for dep in ctx.attr.data]).to_list():
        if "/.aspect_rules_js/" in f.path:
            # Special handling for virtual store deps; we only include 1st party deps since copying
            # all 3rd party node_modules over is expensive for typical graphs
            path_segments = f.path.split("/")
            package_name_segment = path_segments.index(".aspect_rules_js") + 1

            # TODO: @0.0.0 is by default the version of all 1p linked packages, however, it can be overridden by users
            # if they are manually linking a 1p package and not using workspace. A more robust solution would be to
            # split handling of 1p and 3p package in the JsInfo provider itself. Other optimizations in the rule set
            # could also be made if that was the case.
            if len(path_segments) > package_name_segment and "@0.0.0" in path_segments[package_name_segment]:
                # include this first party linked dependency
                data_files.append(f)
        else:
            data_files.append(f)

    config = {
        "data_files": [f.short_path for f in data_files],
    }

    runfiles_merge_targets = ctx.attr.data[:]

    if use_tool:
        if ctx.attr.use_execroot_entry_point:
            config["tool"] = ctx.executable.tool_target_cfg.short_path
            config["use_execroot_entry_point"] = "1"
            config["bazel_bindir"] = ctx.bin_dir.path
            if ctx.attr.allow_execroot_entry_point_with_no_copy_data_to_bin:
                config["allow_execroot_entry_point_with_no_copy_data_to_bin"] = "1"
            runfiles_merge_targets.append(ctx.attr.tool_target_cfg)
        else:
            config["tool"] = ctx.executable.tool_exec_cfg.short_path
            runfiles_merge_targets.append(ctx.attr.tool_exec_cfg)
    if ctx.attr.command:
        config["command"] = ctx.attr.command
    if ctx.attr.grant_sandbox_write_permissions:
        config["grant_sandbox_write_permissions"] = "1"

    ctx.actions.write(config_file, json.encode(config))

    runfiles = ctx.runfiles(
        files = ctx.files.data + [config_file],
        transitive_files = depset(transitive = transitive_runfiles),
    ).merge(launcher.runfiles).merge_all([
        target[DefaultInfo].default_runfiles
        for target in runfiles_merge_targets
    ])

    return [
        DefaultInfo(
            executable = launcher.executable,
            runfiles = runfiles,
        ),
    ]

js_run_devserver_lib = struct(
    attrs = _attrs,
    implementation = _js_run_devserver_impl,
    toolchains = js_binary_lib.toolchains,
)

_js_run_devserver = rule(
    attrs = js_run_devserver_lib.attrs,
    implementation = js_run_devserver_lib.implementation,
    toolchains = js_run_devserver_lib.toolchains,
    executable = True,
)

def js_run_devserver(
        name,
        tool = None,
        command = None,
        grant_sandbox_write_permissions = False,
        use_execroot_entry_point = True,
        allow_execroot_entry_point_with_no_copy_data_to_bin = False,
        **kwargs):
    """Runs a devserver via binary target or command.

    A simple http-server, for example, can be setup as follows,

    ```
    load("@aspect_rules_js//js:defs.bzl", "js_run_devserver")
    load("@npm//:http-server/package_json.bzl", http_server_bin = "bin")

    http_server_bin.http_server_binary(
        name = "http_server",
    )

    js_run_devserver(
        name = "serve",
        args = ["."],
        data = ["index.html"],
        tool = ":http_server",
    )
    ```

    A Next.js devserver can be setup as follows,

    ```
    js_run_devserver(
        name = "dev",
        args = ["dev"],
        command = "./node_modules/.bin/next",
        data = [
            "next.config.js",
            "package.json",
            ":node_modules/next",
            ":node_modules/react",
            ":node_modules/react-dom",
            ":node_modules/typescript",
            "//pages",
            "//public",
            "//styles",
        ],
    )
    ```

    where the `./node_modules/.bin/next` bin entry of Next.js is configured in
    `npm_translate_lock` as such,

    ```
    npm_translate_lock(
        name = "npm",
        bins = {
            # derived from "bin" attribute in node_modules/next/package.json
            "next": {
                "next": "./dist/bin/next",
            },
        },
        pnpm_lock = "//:pnpm-lock.yaml",
    )
    ```

    and run in watch mode using [ibazel](https://github.com/bazelbuild/bazel-watcher) with
    `ibazel run //:dev`.

    The devserver specified by either `tool` or `command` is run in a custom sandbox that is more
    compatible with devserver watch modes in Node.js tools such as Webpack and Next.js.

    The custom sandbox is populated with the default outputs of all targets in `data`
    as well as transitive sources & npm links.

    As an optimization, virtual store files are explicitly excluded from the sandbox since the npm
    links will point to the virtual store in the execroot and Node.js will follow those links as it
    does within the execroot. As a result, rules_js npm package link targets such as
    `//:node_modules/next` are handled efficiently. Since these targets are symlinks in the output
    tree, they are recreated as symlinks in the custom sandbox and do not incur a full copy of the
    underlying npm packages.

    Supports running with [ibazel](https://github.com/bazelbuild/bazel-watcher).
    Only `data` files that change on incremental builds are synchronized when running with ibazel.

    Args:
        name: A unique name for this target.

        tool: The devserver binary target to run.

            Only one of `command` or `tool` may be specified.

        command: The devserver command to run.

            For example, this could be the bin entry of an npm package that is included
            in data such as `./node_modules/.bin/next`.

            Using the bin entry of next, for example, resolves issues with Next.js and React
            being found in multiple node_modules trees when next is run as an encapsulated
            `js_binary` tool.

            Only one of `command` or `tool` may be specified.

        grant_sandbox_write_permissions: If set, write permissions is set on all files copied to the custom sandbox.

            This can be useful to support some devservers such as Next.js which may, under some
            circumstances, try to modify files when running.

            See https://github.com/aspect-build/rules_js/issues/935 for more context.

        use_execroot_entry_point: Use the `entry_point` script of the `js_binary` `tool` that is in the execroot output tree
            instead of the copy that is in runfiles.

            Using the entry point script that is in the execroot output tree means that there will be no conflicting
            runfiles `node_modules` in the node_modules resolution path which can confuse npm packages such as next and
            react that don't like being resolved in multiple node_modules trees. This more closely emulates the
            environment that tools such as Next.js see when they are run outside of Bazel.

            When True, the `js_binary` tool must have `copy_data_to_bin` set to True (the default) so that all data files
            needed by the binary are available in the execroot output tree. This requirement can be turned off with by
            setting `allow_execroot_entry_point_with_no_copy_data_to_bin` to True.

        allow_execroot_entry_point_with_no_copy_data_to_bin: Turn off validation that the `js_binary` tool
            has `copy_data_to_bin` set to True when `use_execroot_entry_point` is set to True.

            See `use_execroot_entry_point` doc for more info.

        **kwargs: All other args from `js_binary` except for `entry_point` which is set implicitly.

            `entry_point` is set implicitly by `js_run_devserver` and cannot be overridden.

            See https://docs.aspect.build/rules/aspect_rules_js/docs/js_binary
    """
    if kwargs.get("entry_point", None):
        fail("`entry_point` is set implicitly by `js_run_devserver` and cannot be overridden.")

    # Allow the js_run_devserver rule to execute to be overridden for tests
    rule_to_execute = kwargs.pop("rule_to_execute", _js_run_devserver)

    rule_to_execute(
        name = name,
        enable_runfiles = select({
            Label("@aspect_rules_js//js:enable_runfiles"): True,
            "//conditions:default": False,
        }),
        unresolved_symlinks_enabled = select({
            Label("@aspect_rules_js//js:allow_unresolved_symlinks"): True,
            "//conditions:default": False,
        }),
        entry_point = Label("@aspect_rules_js//js/private:js_devserver_entrypoint"),
        # This rule speaks the ibazel protocol
        tags = kwargs.pop("tags", []) + ["ibazel_notify_changes"],
        tool_exec_cfg = tool,
        tool_target_cfg = tool,
        command = command,
        grant_sandbox_write_permissions = grant_sandbox_write_permissions,
        use_execroot_entry_point = use_execroot_entry_point,
        allow_execroot_entry_point_with_no_copy_data_to_bin = allow_execroot_entry_point_with_no_copy_data_to_bin,
        **kwargs
    )
