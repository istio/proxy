"""Rules for running JavaScript programs under Bazel, as tools or with `bazel run` or `bazel test`.

For example, this binary references the `acorn` npm package which was already linked
using an API like `npm_link_all_packages`.

```starlark
load("@aspect_rules_js//js:defs.bzl", "js_binary", "js_test")

js_binary(
    name = "bin",
    # Reference the location where the acorn npm module was linked in the root Bazel package
    data = ["//:node_modules/acorn"],
    entry_point = "require_acorn.js",
)
```
"""

load("@aspect_bazel_lib//lib:windows_utils.bzl", "create_windows_native_launcher_script")
load("@aspect_bazel_lib//lib:expand_make_vars.bzl", "expand_locations", "expand_variables")
load("@aspect_bazel_lib//lib:directory_path.bzl", "DirectoryPathInfo")
load("@aspect_bazel_lib//lib:utils.bzl", "is_bazel_6_or_greater")
load("@aspect_bazel_lib//lib:copy_to_bin.bzl", "COPY_FILE_TO_BIN_TOOLCHAINS")
load("@bazel_skylib//lib:dicts.bzl", "dicts")
load(":js_helpers.bzl", "LOG_LEVELS", "envs_for_log_level", "gather_runfiles")
load(":bash.bzl", "BASH_INITIALIZE_RUNFILES")

_DOC = """Execute a program in the Node.js runtime.

The version of Node.js is determined by Bazel's toolchain selection. In the WORKSPACE you used
`nodejs_register_toolchains` to provide options to Bazel. Then Bazel selects from these options
based on the requested target platform. Use the
[`--toolchain_resolution_debug`](https://docs.bazel.build/versions/main/command-line-reference.html#flag--toolchain_resolution_debug)
Bazel option to see more detail about the selection.

All [common binary attributes](https://bazel.build/reference/be/common-definitions#common-attributes-binaries) are supported
including `args` as the list of arguments passed Node.js.

The following environment variables are made available to the Node.js runtime based on available Bazel [Make variables](https://bazel.build/reference/be/make-variables#predefined_variables):

* JS_BINARY__BINDIR: the WORKSPACE-relative Bazel bin directory; equivalent to the `$(BINDIR)` Make variable of the `js_binary` target
* JS_BINARY__COMPILATION_MODE: One of `fastbuild`, `dbg`, or `opt` as set by [`--compilation_mode`](https://bazel.build/docs/user-manual#compilation-mode); equivalent to `$(COMPILATION_MODE)` Make variable of the `js_binary` target
* JS_BINARY__TARGET_CPU: the target cpu architecture; equivalent to `$(TARGET_CPU)` Make variable of the `js_binary` target

The following environment variables are made available to the Node.js runtime based on the rule context:

* JS_BINARY__BUILD_FILE_PATH: the WORKSPACE-relative path to the BUILD file of the Bazel target being run; equivalent to `ctx.build_file_path` of the `js_binary` target's rule context
* JS_BINARY__PACKAGE: the package of the Bazel target being run; equivalent to `ctx.label.package` of the `js_binary` target's rule context
* JS_BINARY__TARGET: the full label of the Bazel target being run; a stringified version of `ctx.label` of the `js_binary` target's rule context
* JS_BINARY__TARGET_NAME: the name of the Bazel target being run; equivalent to `ctx.label.name` of the `js_binary` target's rule context
* JS_BINARY__WORKSPACE: the Bazel workspace name; equivalent to `ctx.workspace_name` of the `js_binary` target's rule context

The following environment variables are made available to the Node.js runtime based the runtime environment:

* JS_BINARY__NODE_BINARY: the Node.js binary path run by the `js_binary` target
* JS_BINARY__NPM_BINARY: the npm binary path; this is available when [`include_npm`](https://docs.aspect.build/rules/aspect_rules_js/docs/js_binary#include_npm) is `True` on the `js_binary` target
* JS_BINARY__NODE_WRAPPER: the Node.js wrapper script used to run Node.js which is available as `node` on the `PATH` at runtime
* JS_BINARY__RUNFILES: the absolute path to the Bazel runfiles directory
* JS_BINARY__EXECROOT: the absolute path to the root of the execution root for the action; if in the sandbox, this path absolute path to the root of the execution root within the sandbox
"""

_ATTRS = {
    "chdir": attr.string(
        doc = """Working directory to run the binary or test in, relative to the workspace.

        By default, `js_binary` runs in the root of the output tree.

        To run in the directory containing the `js_binary` use

            chdir = package_name()

        (or if you're in a macro, use `native.package_name()`)

        WARNING: this will affect other paths passed to the program, either as arguments or in configuration files,
        which are workspace-relative.

        You may need `../../` segments to re-relativize such paths to the new working directory.
        In a `BUILD` file you could do something like this to point to the output path:

        ```python
        js_binary(
            ...
            chdir = package_name(),
            # ../.. segments to re-relative paths from the chdir back to workspace;
            # add an additional 3 segments to account for running js_binary running
            # in the root of the output tree
            args = ["/".join([".."] * len(package_name().split("/"))) + "$(rootpath //path/to/some:file)"],
        )
        ```""",
    ),
    "data": attr.label_list(
        allow_files = True,
        doc = """Runtime dependencies of the program.

        The transitive closure of the `data` dependencies will be available in
        the .runfiles folder for this binary/test.

        NB: `data` files are copied to the Bazel output tree before being passed
        as inputs to runfiles. See `copy_data_to_bin` docstring for more info.
        """,
    ),
    "entry_point": attr.label(
        allow_files = True,
        doc = """The main script which is evaluated by node.js.

        This is the module referenced by the `require.main` property in the runtime.

        This must be a target that provides a single file or a `DirectoryPathInfo`
        from `@aspect_bazel_lib//lib::directory_path.bzl`.
        
        See https://github.com/aspect-build/bazel-lib/blob/main/docs/directory_path.md
        for more info on creating a target that provides a `DirectoryPathInfo`.
        """,
        mandatory = True,
    ),
    "enable_runfiles": attr.bool(
        mandatory = True,
        doc = """Whether runfiles are enabled in the current build configuration.

        Typical usage of this rule is via a macro which automatically sets this
        attribute based on a `config_setting` rule.
        """,
    ),
    "env": attr.string_dict(
        doc = """Environment variables of the action.

        Subject to [$(location)](https://bazel.build/reference/be/make-variables#predefined_label_variables)
        and ["Make variable"](https://bazel.build/reference/be/make-variables) substitution.
        """,
    ),
    "fixed_args": attr.string_list(
        doc = """Fixed command line arguments to pass to the Node.js when this
        binary target is executed.

        Subject to [$(location)](https://bazel.build/reference/be/make-variables#predefined_label_variables)
        and ["Make variable"](https://bazel.build/reference/be/make-variables) substitution.

        Unlike the built-in `args`, which are only passed to the target when it is
        executed either by the `bazel run` command or as a test, `fixed_args` are baked
        into the generated launcher script so are always passed even when the binary
        target is run outside of Bazel directly from the launcher script.

        `fixed_args` are passed before the ones specified in `args` and before ones
        that are specified on the `bazel run` or `bazel test` command line.

        See https://bazel.build/reference/be/common-definitions#common-attributes-binaries
        for more info on the built-in `args` attribute.
        """,
    ),
    "node_options": attr.string_list(
        doc = """Options to pass to the node invocation on the command line.

        https://nodejs.org/api/cli.html

        These options are passed directly to the node invocation on the command line.
        Options passed here will take precendence over options passed via
        the NODE_OPTIONS environment variable. Options passed here are not added
        to the NODE_OPTIONS environment variable so will not be automatically
        picked up by child processes that inherit that enviroment variable.
        """,
    ),
    "expected_exit_code": attr.int(
        doc = """The expected exit code.

        Can be used to write tests that are expected to fail.""",
        default = 0,
    ),
    "log_level": attr.string(
        doc = """Set the logging level.

        Log from are written to stderr. They will be supressed on success when running as the tool
        of a js_run_binary when silent_on_success is True. In that case, they will be shown
        only on a build failure along with the stdout & stderr of the node tool being run.

        Log levels: {}""".format(", ".join(LOG_LEVELS.keys())),
        values = LOG_LEVELS.keys(),
        default = "error",
    ),
    "patch_node_fs": attr.bool(
        doc = """Patch the to Node.js `fs` API (https://nodejs.org/api/fs.html) for this node program
        to prevent the program from following symlinks out of the execroot, runfiles and the sandbox.

        When enabled, `js_binary` patches the Node.js sync and async `fs` API functions `lstat`,
        `readlink`, `realpath`, `readdir` and `opendir` so that the node program being
        run cannot resolve symlinks out of the execroot and the runfiles tree. When in the sandbox,
        these patches prevent the program being run from resolving symlinks out of the sandbox.

        When disabled, node programs can leave the execroot, runfiles and sandbox by following symlinks
        which can lead to non-hermetic behavior.""",
        default = True,
    ),
    "include_transitive_sources": attr.bool(
        doc = """When True, `transitive_sources` from `JsInfo` providers in data targets are included in the runfiles of the target.""",
        default = True,
    ),
    "include_declarations": attr.bool(
        doc = """When True, `declarations` and `transitive_declarations` from `JsInfo` providers in data targets are included in the runfiles of the target.

        Defaults to false since declarations are generally not needed at runtime and introducing them could slow down developer round trip
        time due to having to generate typings on source file changes.""",
        default = False,
    ),
    "include_npm_linked_packages": attr.bool(
        doc = """When True, files in `npm_linked_packages` and `transitive_npm_linked_packages` from `JsInfo` providers in data targets are included in the runfiles of the target.

        `transitive_files` from `NpmPackageStoreInfo` providers in data targets are also included in the runfiles of the target.
        """,
        default = True,
    ),
    "preserve_symlinks_main": attr.bool(
        doc = """When True, the --preserve-symlinks-main flag is passed to node.

        This prevents node from following an ESM entry script out of runfiles and the sandbox. This can happen for `.mjs`
        ESM entry points where the fs node patches, which guard the runfiles and sandbox, are not applied.
        See https://github.com/aspect-build/rules_js/issues/362 for more information. Once #362 is resolved,
        the default for this attribute can be set to False.

        This flag was added in Node.js v10.2.0 (released 2018-05-23). If your node toolchain is configured to use a
        Node.js version older than this you'll need to set this attribute to False.

        See https://nodejs.org/api/cli.html#--preserve-symlinks-main for more information.
        """,
        default = True,
    ),
    "no_copy_to_bin": attr.label_list(
        allow_files = True,
        doc = """List of files to not copy to the Bazel output tree when `copy_data_to_bin` is True.

        This is useful for exceptional cases where a `copy_to_bin` is not possible or not suitable for an input
        file such as a file in an external repository. In most cases, this option is not needed.
        See `copy_data_to_bin` docstring for more info.
        """,
    ),
    "copy_data_to_bin": attr.bool(
        doc = """When True, `data` files and the `entry_point` file are copied to the Bazel output tree before being passed
        as inputs to runfiles.

        Defaults to True so that a `js_binary` with the default value is compatible with `js_run_binary` with
        `use_execroot_entry_point` set to True, the default there.

        Setting this to False is more optimal in terms of inputs, but there is a yet unresolved issue of ESM imports
        skirting the node fs patches and escaping the sandbox: https://github.com/aspect-build/rules_js/issues/362.
        This is hit in some popular test runners such as mocha, which use native `import()` statements
        (https://github.com/aspect-build/rules_js/pull/353). When set to False, a program such as mocha that uses ESM
        imports may escape the execroot by following symlinks into the source tree. When set to True, such a program
        would escape the sandbox but will end up in the output tree where `node_modules` and other inputs required
        will be available.
        """,
        default = True,
    ),
    "include_npm": attr.bool(
        doc = """When True, npm is included in the runfiles of the target.

        An npm binary is also added on the PATH so tools can spawn npm processes. This is a bash script
        on Linux and MacOS and a batch script on Windows.
        
        A minimum of rules_nodejs version 5.7.0 is required which contains the Node.js toolchain changes
        to use npm.
        """,
    ),
    "unresolved_symlinks_enabled": attr.bool(
        doc = """Whether unresolved symlinks are enabled in the current build configuration.

        These are enabled with the `--allow_unresolved_symlinks` flag
        (named `--experimental_allow_unresolved_symlinks in Bazel versions prior to 7.0).

        Typical usage of this rule is via a macro which automatically sets this
        attribute based on a `config_setting` rule.
        See /js/private/BUILD.bazel in rules_js for an example.
        """,
        # TODO(2.0): make this mandatory so that downstream binary rules that inherit these attributes are required to set it
        mandatory = False,
    ),
    "node_toolchain": attr.label(
        doc = """The Node.js toolchain to use for this target.

        See https://bazelbuild.github.io/rules_nodejs/Toolchains.html

        Typically this is left unset so that Bazel automatically selects the right Node.js toolchain
        for the target platform. See https://bazel.build/extending/toolchains#toolchain-resolution
        for more information.
        """,
    ),
    "_launcher_template": attr.label(
        default = Label("//js/private:js_binary.sh.tpl"),
        allow_single_file = True,
    ),
    "_node_wrapper_sh": attr.label(
        default = Label("//js/private:node_wrapper.sh"),
        allow_single_file = True,
    ),
    "_node_wrapper_bat": attr.label(
        default = Label("//js/private:node_wrapper.bat"),
        allow_single_file = True,
    ),
    "_npm_wrapper_sh": attr.label(
        default = Label("//js/private:npm_wrapper.sh"),
        allow_single_file = True,
    ),
    "_npm_wrapper_bat": attr.label(
        default = Label("//js/private:npm_wrapper.bat"),
        allow_single_file = True,
    ),
    "_windows_constraint": attr.label(default = "@platforms//os:windows"),
    "_node_patches_legacy_files": attr.label_list(
        allow_files = True,
        default = [Label("@aspect_rules_js//js/private/node-patches_legacy:fs.js")],
    ),
    "_node_patches_legacy": attr.label(
        allow_single_file = True,
        default = Label("@aspect_rules_js//js/private/node-patches_legacy:register.js"),
    ),
    "_node_patches_files": attr.label_list(
        allow_files = True,
        default = [Label("@aspect_rules_js//js/private/node-patches:fs.js")],
    ),
    "_node_patches": attr.label(
        allow_single_file = True,
        default = Label("@aspect_rules_js//js/private/node-patches:register.js"),
    ),
}

_ENV_SET = """export {var}=\"{value}\""""
_ENV_SET_IFF_NOT_SET = """if [[ -z "${{{var}:-}}" ]]; then export {var}=\"{value}\"; fi"""
_NODE_OPTION = """JS_BINARY__NODE_OPTIONS+=(\"{value}\")"""

# Do the opposite of _to_manifest_path in
# https://github.com/bazelbuild/rules_nodejs/blob/8b5d27400db51e7027fe95ae413eeabea4856f8e/nodejs/toolchain.bzl#L50
# to get back to the short_path.
# TODO(3.0): remove this after a grace period for the DEPRECATED toolchain attributes
# buildifier: disable=unused-variable
def _deprecated_target_tool_path_to_short_path(tool_path):
    return ("../" + tool_path[len("external/"):]) if tool_path.startswith("external/") else tool_path

# Generate a consistent label string between Bazel versions.
# TODO(2.0): hoist this function to bazel-lib and use from there (as well as the dup in npm/private/utils.bzl)
def _consistent_label_str(workspace_name, label):
    # Starting in Bazel 6, the workspace name is empty for the local workspace and there's no other way to determine it.
    # This behavior differs from Bazel 5 where the local workspace name was fully qualified in str(label).
    workspace_name = "" if label.workspace_name == workspace_name else label.workspace_name
    return "@{}//{}:{}".format(
        workspace_name,
        label.package,
        label.name,
    )

def _bash_launcher(ctx, nodeinfo, entry_point_path, log_prefix_rule_set, log_prefix_rule, fixed_args, fixed_env, is_windows, use_legacy_node_patches):
    # Explicitly disable node fs patches on Windows:
    # https://github.com/aspect-build/rules_js/issues/1137
    if is_windows:
        fixed_env = dict(fixed_env, **{"JS_BINARY__PATCH_NODE_FS": "0"})

    envs = []
    for (key, value) in dicts.add(fixed_env, ctx.attr.env).items():
        envs.append(_ENV_SET.format(
            var = key,
            value = " ".join([expand_variables(ctx, exp, attribute_name = "env") for exp in expand_locations(ctx, value, ctx.attr.data).split(" ")]),
        ))

    # Add common and useful make variables to the environment
    makevars = {
        "JS_BINARY__BINDIR": "$(BINDIR)",
        "JS_BINARY__COMPILATION_MODE": "$(COMPILATION_MODE)",
        "JS_BINARY__TARGET_CPU": "$(TARGET_CPU)",
    }
    for (key, value) in makevars.items():
        envs.append(_ENV_SET.format(
            var = key,
            value = ctx.expand_make_variables("env", value, {}),
        ))

    # Add rule context variables to the environment
    builtins = {
        "JS_BINARY__BUILD_FILE_PATH": ctx.build_file_path,
        "JS_BINARY__PACKAGE": ctx.label.package,
        "JS_BINARY__TARGET_NAME": ctx.label.name,
        "JS_BINARY__TARGET": "{}//{}:{}".format(
            "@" + ctx.label.workspace_name if ctx.label.workspace_name else "",
            ctx.label.package,
            ctx.label.name,
        ),
        "JS_BINARY__WORKSPACE": ctx.workspace_name,
    }
    if is_windows and not ctx.attr.enable_runfiles:
        builtins["JS_BINARY__NO_RUNFILES"] = "1"
    for (key, value) in builtins.items():
        envs.append(_ENV_SET.format(var = key, value = value))

    if ctx.attr.patch_node_fs:
        # Set patch node fs API env if not already set to allow js_run_binary to override
        envs.append(_ENV_SET_IFF_NOT_SET.format(var = "JS_BINARY__PATCH_NODE_FS", value = "1"))

    if ctx.attr.expected_exit_code:
        envs.append(_ENV_SET.format(
            var = "JS_BINARY__EXPECTED_EXIT_CODE",
            value = str(ctx.attr.expected_exit_code),
        ))

    if ctx.attr.copy_data_to_bin:
        # Set an environment variable to flag that we have copied js_binary data to bin
        envs.append(_ENV_SET.format(var = "JS_BINARY__COPY_DATA_TO_BIN", value = "1"))

    if ctx.attr.chdir:
        # Set chdir env if not already set to allow js_run_binary to override
        envs.append(_ENV_SET_IFF_NOT_SET.format(
            var = "JS_BINARY__CHDIR",
            value = " ".join([expand_variables(ctx, exp, attribute_name = "env") for exp in expand_locations(ctx, ctx.attr.chdir, ctx.attr.data).split(" ")]),
        ))

    # Set log envs iff not already set to allow js_run_binary to override
    for env in envs_for_log_level(ctx.attr.log_level):
        envs.append(_ENV_SET_IFF_NOT_SET.format(var = env, value = "1"))

    node_options = []
    for node_option in ctx.attr.node_options:
        node_options.append(_NODE_OPTION.format(
            value = " ".join([expand_variables(ctx, exp, attribute_name = "env") for exp in expand_locations(ctx, node_option, ctx.attr.data).split(" ")]),
        ))
    if ctx.attr.preserve_symlinks_main and "--preserve-symlinks-main" not in node_options:
        node_options.append(_NODE_OPTION.format(value = "--preserve-symlinks-main"))

    fixed_args_expanded = [expand_variables(ctx, expand_locations(ctx, fixed_arg, ctx.attr.data)) for fixed_arg in fixed_args]

    toolchain_files = []
    if is_windows:
        node_wrapper = ctx.actions.declare_file("%s_node_bin/node.bat" % ctx.label.name)
        ctx.actions.expand_template(
            template = ctx.file._node_wrapper_bat,
            output = node_wrapper,
            substitutions = {},
            is_executable = True,
        )
    else:
        node_wrapper = ctx.actions.declare_file("%s_node_bin/node" % ctx.label.name)
        ctx.actions.expand_template(
            template = ctx.file._node_wrapper_sh,
            output = node_wrapper,
            substitutions = {},
            is_executable = True,
        )
    toolchain_files.append(node_wrapper)

    npm_path = ""
    if ctx.attr.include_npm:
        if hasattr(nodeinfo, "npm"):
            npm_path = nodeinfo.npm.short_path if nodeinfo.npm else nodeinfo.npm_path
        else:
            # TODO(3.0): drop support for deprecated toolchain attributes
            npm_path = _deprecated_target_tool_path_to_short_path(nodeinfo.npm_path)
        if is_windows:
            npm_wrapper = ctx.actions.declare_file("%s_node_bin/npm.bat" % ctx.label.name)
            ctx.actions.expand_template(
                template = ctx.file._npm_wrapper_bat,
                output = npm_wrapper,
                substitutions = {},
                is_executable = True,
            )
        else:
            npm_wrapper = ctx.actions.declare_file("%s_node_bin/npm" % ctx.label.name)
            ctx.actions.expand_template(
                template = ctx.file._npm_wrapper_sh,
                output = npm_wrapper,
                substitutions = {},
                is_executable = True,
            )
        toolchain_files.append(npm_wrapper)

    if hasattr(nodeinfo, "node"):
        node_path = nodeinfo.node.short_path if nodeinfo.node else nodeinfo.node_path
    else:
        # TODO(3.0): drop support for deprecated toolchain attributes
        node_path = _deprecated_target_tool_path_to_short_path(nodeinfo.target_tool_path)

    launcher_subst = {
        "{{target_label}}": _consistent_label_str(ctx.workspace_name, ctx.label),
        "{{template_label}}": _consistent_label_str(ctx.workspace_name, ctx.attr._launcher_template.label),
        "{{entry_point_label}}": _consistent_label_str(ctx.workspace_name, ctx.attr.entry_point.label),
        "{{entry_point_path}}": entry_point_path,
        "{{envs}}": "\n".join(envs),
        "{{fixed_args}}": " ".join(fixed_args_expanded),
        "{{initialize_runfiles}}": BASH_INITIALIZE_RUNFILES,
        "{{log_prefix_rule_set}}": log_prefix_rule_set,
        "{{log_prefix_rule}}": log_prefix_rule,
        "{{node_options}}": "\n".join(node_options),
        "{{node_patches}}": ctx.file._node_patches_legacy.short_path if use_legacy_node_patches else ctx.file._node_patches.short_path,
        "{{node_wrapper}}": node_wrapper.short_path,
        "{{node}}": node_path,
        "{{npm}}": npm_path,
        "{{workspace_name}}": ctx.workspace_name,
    }

    launcher = ctx.actions.declare_file("%s.sh" % ctx.label.name)
    ctx.actions.expand_template(
        template = ctx.file._launcher_template,
        output = launcher,
        substitutions = launcher_subst,
        is_executable = True,
    )

    return launcher, toolchain_files

def _create_launcher(ctx, log_prefix_rule_set, log_prefix_rule, fixed_args = [], fixed_env = {}):
    is_windows = ctx.target_platform_has_constraint(ctx.attr._windows_constraint[platform_common.ConstraintValueInfo])
    is_bazel_6 = is_bazel_6_or_greater()
    unresolved_symlinks_enabled = False
    if hasattr(ctx.attr, "unresolved_symlinks_enabled"):
        unresolved_symlinks_enabled = ctx.attr.unresolved_symlinks_enabled
    use_legacy_node_patches = not is_bazel_6 or not unresolved_symlinks_enabled

    if ctx.attr.node_toolchain:
        nodeinfo = ctx.attr.node_toolchain[platform_common.ToolchainInfo].nodeinfo
    else:
        nodeinfo = ctx.toolchains["@rules_nodejs//nodejs:toolchain_type"].nodeinfo

    if DirectoryPathInfo in ctx.attr.entry_point:
        entry_point = ctx.attr.entry_point[DirectoryPathInfo].directory
        entry_point_path = "/".join([
            ctx.attr.entry_point[DirectoryPathInfo].directory.short_path,
            ctx.attr.entry_point[DirectoryPathInfo].path,
        ])
    else:
        if len(ctx.files.entry_point) != 1:
            fail("entry_point must be a single file or a target that provides a DirectoryPathInfo")
        entry_point = ctx.files.entry_point[0]
        entry_point_path = entry_point.short_path

    bash_launcher, toolchain_files = _bash_launcher(ctx, nodeinfo, entry_point_path, log_prefix_rule_set, log_prefix_rule, fixed_args, fixed_env, is_windows, use_legacy_node_patches)
    launcher = create_windows_native_launcher_script(ctx, bash_launcher) if is_windows else bash_launcher

    launcher_files = [bash_launcher]
    launcher_files.extend(toolchain_files)
    if hasattr(nodeinfo, "node"):
        if nodeinfo.node:
            launcher_files.append(nodeinfo.node)
    else:
        # TODO(3.0): drop support for deprecated toolchain attributes
        launcher_files.extend(nodeinfo.tool_files)

    if use_legacy_node_patches:
        launcher_files.extend(ctx.files._node_patches_legacy_files + [ctx.file._node_patches_legacy])
    else:
        launcher_files.extend(ctx.files._node_patches_files + [ctx.file._node_patches])
    transitive_launcher_files = None
    if ctx.attr.include_npm:
        if hasattr(nodeinfo, "npm_sources"):
            transitive_launcher_files = nodeinfo.npm_sources
        else:
            # TODO(3.0): drop support for deprecated toolchain attributes
            if not hasattr(nodeinfo, "npm_files"):
                fail("include_npm requires a minimum @rules_nodejs version of 5.7.0")
            launcher_files.extend(nodeinfo.npm_files)

    runfiles = gather_runfiles(
        ctx = ctx,
        sources = [],
        data = ctx.attr.data,
        data_files = [entry_point] + ctx.files.data,
        deps = [],
        copy_data_files_to_bin = ctx.attr.copy_data_to_bin,
        no_copy_to_bin = ctx.files.no_copy_to_bin,
        include_transitive_sources = ctx.attr.include_transitive_sources,
        include_declarations = ctx.attr.include_declarations,
        include_npm_linked_packages = ctx.attr.include_npm_linked_packages,
    ).merge(ctx.runfiles(
        files = launcher_files,
        transitive_files = transitive_launcher_files,
    ))

    return struct(
        executable = launcher,
        runfiles = runfiles,
    )

def _js_binary_impl(ctx):
    launcher = _create_launcher(
        ctx,
        log_prefix_rule_set = "aspect_rules_js",
        log_prefix_rule = "js_test" if ctx.attr.testonly else "js_binary",
        fixed_args = ctx.attr.fixed_args,
    )
    runfiles = launcher.runfiles

    providers = []

    if ctx.attr.testonly and ctx.configuration.coverage_enabled:
        # We have to instruct rule implementers to have this attribute present.
        if not hasattr(ctx.attr, "_lcov_merger"):
            fail("_lcov_merger attribute is missing and coverage was requested")

        # We have to propagate _lcov_merger runfiles since bazel does not treat _lcov_merger as a proper tool.
        # See: https://github.com/bazelbuild/bazel/issues/4033
        runfiles = runfiles.merge(ctx.attr._lcov_merger[DefaultInfo].default_runfiles)
        providers = [
            coverage_common.instrumented_files_info(
                ctx,
                source_attributes = ["data"],
                # TODO: check if there is more extensions
                # TODO: .ts should not be here since we ought to only instrument transpiled files?
                extensions = [
                    "mjs",
                    "mts",
                    "cjs",
                    "cts",
                    "ts",
                    "js",
                    "jsx",
                    "tsx",
                ],
            ),
        ]

    return providers + [
        DefaultInfo(
            executable = launcher.executable,
            runfiles = runfiles,
        ),
    ]

js_binary_lib = struct(
    attrs = _ATTRS,
    create_launcher = _create_launcher,
    implementation = _js_binary_impl,
    toolchains = [
        # TODO: on Windows this toolchain is never referenced
        "@bazel_tools//tools/sh:toolchain_type",
        "@rules_nodejs//nodejs:toolchain_type",
    ] + COPY_FILE_TO_BIN_TOOLCHAINS,
)

js_binary = rule(
    doc = _DOC,
    implementation = js_binary_lib.implementation,
    attrs = js_binary_lib.attrs,
    executable = True,
    toolchains = js_binary_lib.toolchains,
)

js_test = rule(
    doc = """Identical to js_binary, but usable under `bazel test`.

All [common test attributes](https://bazel.build/reference/be/common-definitions#common-attributes-tests) are
supported including `args` as the list of arguments passed Node.js.

Bazel will set environment variables when a test target is run under `bazel test` and `bazel run`
that a test runner can use.

A runner can write arbitrary outputs files it wants Bazel to pickup and save with the test logs to
`TEST_UNDECLARED_OUTPUTS_DIR`. These get zipped up and saved along with the test logs.

JUnit XML reports can be written to `XML_OUTPUT_FILE` for Bazel to consume.

`TEST_TMPDIR` is an absolute path to a private writeable directory that the test runner can use for
creating temporary files.

LCOV coverage reports can be written to `COVERAGE_OUTPUT_FILE` when running under `bazel coverage`
or if the `--coverage` flag is set.

See the Bazel [Test encyclopedia](https://bazel.build/reference/test-encyclopedia) for details on
the contract between Bazel and a test runner.""",
    implementation = js_binary_lib.implementation,
    attrs = dict(js_binary_lib.attrs, **{
        "_lcov_merger": attr.label(
            executable = True,
            default = Label("//js/private/coverage:merger"),
            cfg = "exec",
        ),
    }),
    test = True,
    toolchains = js_binary_lib.toolchains,
)
