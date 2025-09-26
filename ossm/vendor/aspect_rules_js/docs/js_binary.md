<!-- Generated with Stardoc: http://skydoc.bazel.build -->

Rules for running JavaScript programs under Bazel, as tools or with `bazel run` or `bazel test`.

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


<a id="js_binary"></a>

## js_binary

<pre>
js_binary(<a href="#js_binary-name">name</a>, <a href="#js_binary-chdir">chdir</a>, <a href="#js_binary-copy_data_to_bin">copy_data_to_bin</a>, <a href="#js_binary-data">data</a>, <a href="#js_binary-enable_runfiles">enable_runfiles</a>, <a href="#js_binary-entry_point">entry_point</a>, <a href="#js_binary-env">env</a>,
          <a href="#js_binary-expected_exit_code">expected_exit_code</a>, <a href="#js_binary-fixed_args">fixed_args</a>, <a href="#js_binary-include_declarations">include_declarations</a>, <a href="#js_binary-include_npm">include_npm</a>,
          <a href="#js_binary-include_npm_linked_packages">include_npm_linked_packages</a>, <a href="#js_binary-include_transitive_sources">include_transitive_sources</a>, <a href="#js_binary-log_level">log_level</a>, <a href="#js_binary-no_copy_to_bin">no_copy_to_bin</a>,
          <a href="#js_binary-node_options">node_options</a>, <a href="#js_binary-node_toolchain">node_toolchain</a>, <a href="#js_binary-patch_node_fs">patch_node_fs</a>, <a href="#js_binary-preserve_symlinks_main">preserve_symlinks_main</a>,
          <a href="#js_binary-unresolved_symlinks_enabled">unresolved_symlinks_enabled</a>)
</pre>

Execute a program in the Node.js runtime.

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


**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="js_binary-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="js_binary-chdir"></a>chdir |  Working directory to run the binary or test in, relative to the workspace.<br><br>        By default, <code>js_binary</code> runs in the root of the output tree.<br><br>        To run in the directory containing the <code>js_binary</code> use<br><br>            chdir = package_name()<br><br>        (or if you're in a macro, use <code>native.package_name()</code>)<br><br>        WARNING: this will affect other paths passed to the program, either as arguments or in configuration files,         which are workspace-relative.<br><br>        You may need <code>../../</code> segments to re-relativize such paths to the new working directory.         In a <code>BUILD</code> file you could do something like this to point to the output path:<br><br>        <pre><code>python         js_binary(             ...             chdir = package_name(),             # ../.. segments to re-relative paths from the chdir back to workspace;             # add an additional 3 segments to account for running js_binary running             # in the root of the output tree             args = ["/".join([".."] * len(package_name().split("/"))) + "$(rootpath //path/to/some:file)"],         )         </code></pre>   | String | optional | <code>""</code> |
| <a id="js_binary-copy_data_to_bin"></a>copy_data_to_bin |  When True, <code>data</code> files and the <code>entry_point</code> file are copied to the Bazel output tree before being passed         as inputs to runfiles.<br><br>        Defaults to True so that a <code>js_binary</code> with the default value is compatible with <code>js_run_binary</code> with         <code>use_execroot_entry_point</code> set to True, the default there.<br><br>        Setting this to False is more optimal in terms of inputs, but there is a yet unresolved issue of ESM imports         skirting the node fs patches and escaping the sandbox: https://github.com/aspect-build/rules_js/issues/362.         This is hit in some popular test runners such as mocha, which use native <code>import()</code> statements         (https://github.com/aspect-build/rules_js/pull/353). When set to False, a program such as mocha that uses ESM         imports may escape the execroot by following symlinks into the source tree. When set to True, such a program         would escape the sandbox but will end up in the output tree where <code>node_modules</code> and other inputs required         will be available.   | Boolean | optional | <code>True</code> |
| <a id="js_binary-data"></a>data |  Runtime dependencies of the program.<br><br>        The transitive closure of the <code>data</code> dependencies will be available in         the .runfiles folder for this binary/test.<br><br>        NB: <code>data</code> files are copied to the Bazel output tree before being passed         as inputs to runfiles. See <code>copy_data_to_bin</code> docstring for more info.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional | <code>[]</code> |
| <a id="js_binary-enable_runfiles"></a>enable_runfiles |  Whether runfiles are enabled in the current build configuration.<br><br>        Typical usage of this rule is via a macro which automatically sets this         attribute based on a <code>config_setting</code> rule.   | Boolean | required |  |
| <a id="js_binary-entry_point"></a>entry_point |  The main script which is evaluated by node.js.<br><br>        This is the module referenced by the <code>require.main</code> property in the runtime.<br><br>        This must be a target that provides a single file or a <code>DirectoryPathInfo</code>         from <code>@aspect_bazel_lib//lib::directory_path.bzl</code>.<br><br>        See https://github.com/aspect-build/bazel-lib/blob/main/docs/directory_path.md         for more info on creating a target that provides a <code>DirectoryPathInfo</code>.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="js_binary-env"></a>env |  Environment variables of the action.<br><br>        Subject to [$(location)](https://bazel.build/reference/be/make-variables#predefined_label_variables)         and ["Make variable"](https://bazel.build/reference/be/make-variables) substitution.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional | <code>{}</code> |
| <a id="js_binary-expected_exit_code"></a>expected_exit_code |  The expected exit code.<br><br>        Can be used to write tests that are expected to fail.   | Integer | optional | <code>0</code> |
| <a id="js_binary-fixed_args"></a>fixed_args |  Fixed command line arguments to pass to the Node.js when this         binary target is executed.<br><br>        Subject to [$(location)](https://bazel.build/reference/be/make-variables#predefined_label_variables)         and ["Make variable"](https://bazel.build/reference/be/make-variables) substitution.<br><br>        Unlike the built-in <code>args</code>, which are only passed to the target when it is         executed either by the <code>bazel run</code> command or as a test, <code>fixed_args</code> are baked         into the generated launcher script so are always passed even when the binary         target is run outside of Bazel directly from the launcher script.<br><br>        <code>fixed_args</code> are passed before the ones specified in <code>args</code> and before ones         that are specified on the <code>bazel run</code> or <code>bazel test</code> command line.<br><br>        See https://bazel.build/reference/be/common-definitions#common-attributes-binaries         for more info on the built-in <code>args</code> attribute.   | List of strings | optional | <code>[]</code> |
| <a id="js_binary-include_declarations"></a>include_declarations |  When True, <code>declarations</code> and <code>transitive_declarations</code> from <code>JsInfo</code> providers in data targets are included in the runfiles of the target.<br><br>        Defaults to false since declarations are generally not needed at runtime and introducing them could slow down developer round trip         time due to having to generate typings on source file changes.   | Boolean | optional | <code>False</code> |
| <a id="js_binary-include_npm"></a>include_npm |  When True, npm is included in the runfiles of the target.<br><br>        An npm binary is also added on the PATH so tools can spawn npm processes. This is a bash script         on Linux and MacOS and a batch script on Windows.<br><br>        A minimum of rules_nodejs version 5.7.0 is required which contains the Node.js toolchain changes         to use npm.   | Boolean | optional | <code>False</code> |
| <a id="js_binary-include_npm_linked_packages"></a>include_npm_linked_packages |  When True, files in <code>npm_linked_packages</code> and <code>transitive_npm_linked_packages</code> from <code>JsInfo</code> providers in data targets are included in the runfiles of the target.<br><br>        <code>transitive_files</code> from <code>NpmPackageStoreInfo</code> providers in data targets are also included in the runfiles of the target.   | Boolean | optional | <code>True</code> |
| <a id="js_binary-include_transitive_sources"></a>include_transitive_sources |  When True, <code>transitive_sources</code> from <code>JsInfo</code> providers in data targets are included in the runfiles of the target.   | Boolean | optional | <code>True</code> |
| <a id="js_binary-log_level"></a>log_level |  Set the logging level.<br><br>        Log from are written to stderr. They will be supressed on success when running as the tool         of a js_run_binary when silent_on_success is True. In that case, they will be shown         only on a build failure along with the stdout & stderr of the node tool being run.<br><br>        Log levels: fatal, error, warn, info, debug   | String | optional | <code>"error"</code> |
| <a id="js_binary-no_copy_to_bin"></a>no_copy_to_bin |  List of files to not copy to the Bazel output tree when <code>copy_data_to_bin</code> is True.<br><br>        This is useful for exceptional cases where a <code>copy_to_bin</code> is not possible or not suitable for an input         file such as a file in an external repository. In most cases, this option is not needed.         See <code>copy_data_to_bin</code> docstring for more info.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional | <code>[]</code> |
| <a id="js_binary-node_options"></a>node_options |  Options to pass to the node invocation on the command line.<br><br>        https://nodejs.org/api/cli.html<br><br>        These options are passed directly to the node invocation on the command line.         Options passed here will take precendence over options passed via         the NODE_OPTIONS environment variable. Options passed here are not added         to the NODE_OPTIONS environment variable so will not be automatically         picked up by child processes that inherit that enviroment variable.   | List of strings | optional | <code>[]</code> |
| <a id="js_binary-node_toolchain"></a>node_toolchain |  The Node.js toolchain to use for this target.<br><br>        See https://bazelbuild.github.io/rules_nodejs/Toolchains.html<br><br>        Typically this is left unset so that Bazel automatically selects the right Node.js toolchain         for the target platform. See https://bazel.build/extending/toolchains#toolchain-resolution         for more information.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional | <code>None</code> |
| <a id="js_binary-patch_node_fs"></a>patch_node_fs |  Patch the to Node.js <code>fs</code> API (https://nodejs.org/api/fs.html) for this node program         to prevent the program from following symlinks out of the execroot, runfiles and the sandbox.<br><br>        When enabled, <code>js_binary</code> patches the Node.js sync and async <code>fs</code> API functions <code>lstat</code>,         <code>readlink</code>, <code>realpath</code>, <code>readdir</code> and <code>opendir</code> so that the node program being         run cannot resolve symlinks out of the execroot and the runfiles tree. When in the sandbox,         these patches prevent the program being run from resolving symlinks out of the sandbox.<br><br>        When disabled, node programs can leave the execroot, runfiles and sandbox by following symlinks         which can lead to non-hermetic behavior.   | Boolean | optional | <code>True</code> |
| <a id="js_binary-preserve_symlinks_main"></a>preserve_symlinks_main |  When True, the --preserve-symlinks-main flag is passed to node.<br><br>        This prevents node from following an ESM entry script out of runfiles and the sandbox. This can happen for <code>.mjs</code>         ESM entry points where the fs node patches, which guard the runfiles and sandbox, are not applied.         See https://github.com/aspect-build/rules_js/issues/362 for more information. Once #362 is resolved,         the default for this attribute can be set to False.<br><br>        This flag was added in Node.js v10.2.0 (released 2018-05-23). If your node toolchain is configured to use a         Node.js version older than this you'll need to set this attribute to False.<br><br>        See https://nodejs.org/api/cli.html#--preserve-symlinks-main for more information.   | Boolean | optional | <code>True</code> |
| <a id="js_binary-unresolved_symlinks_enabled"></a>unresolved_symlinks_enabled |  Whether unresolved symlinks are enabled in the current build configuration.<br><br>        These are enabled with the <code>--allow_unresolved_symlinks</code> flag         (named <code>--experimental_allow_unresolved_symlinks in Bazel versions prior to 7.0).<br><br>        Typical usage of this rule is via a macro which automatically sets this         attribute based on a </code>config_setting<code> rule.         See /js/private/BUILD.bazel in rules_js for an example.   | Boolean | optional | <code>False</code> |


<a id="js_test"></a>

## js_test

<pre>
js_test(<a href="#js_test-name">name</a>, <a href="#js_test-chdir">chdir</a>, <a href="#js_test-copy_data_to_bin">copy_data_to_bin</a>, <a href="#js_test-data">data</a>, <a href="#js_test-enable_runfiles">enable_runfiles</a>, <a href="#js_test-entry_point">entry_point</a>, <a href="#js_test-env">env</a>, <a href="#js_test-expected_exit_code">expected_exit_code</a>,
        <a href="#js_test-fixed_args">fixed_args</a>, <a href="#js_test-include_declarations">include_declarations</a>, <a href="#js_test-include_npm">include_npm</a>, <a href="#js_test-include_npm_linked_packages">include_npm_linked_packages</a>,
        <a href="#js_test-include_transitive_sources">include_transitive_sources</a>, <a href="#js_test-log_level">log_level</a>, <a href="#js_test-no_copy_to_bin">no_copy_to_bin</a>, <a href="#js_test-node_options">node_options</a>, <a href="#js_test-node_toolchain">node_toolchain</a>,
        <a href="#js_test-patch_node_fs">patch_node_fs</a>, <a href="#js_test-preserve_symlinks_main">preserve_symlinks_main</a>, <a href="#js_test-unresolved_symlinks_enabled">unresolved_symlinks_enabled</a>)
</pre>

Identical to js_binary, but usable under `bazel test`.

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
the contract between Bazel and a test runner.

**ATTRIBUTES**


| Name  | Description | Type | Mandatory | Default |
| :------------- | :------------- | :------------- | :------------- | :------------- |
| <a id="js_test-name"></a>name |  A unique name for this target.   | <a href="https://bazel.build/concepts/labels#target-names">Name</a> | required |  |
| <a id="js_test-chdir"></a>chdir |  Working directory to run the binary or test in, relative to the workspace.<br><br>        By default, <code>js_binary</code> runs in the root of the output tree.<br><br>        To run in the directory containing the <code>js_binary</code> use<br><br>            chdir = package_name()<br><br>        (or if you're in a macro, use <code>native.package_name()</code>)<br><br>        WARNING: this will affect other paths passed to the program, either as arguments or in configuration files,         which are workspace-relative.<br><br>        You may need <code>../../</code> segments to re-relativize such paths to the new working directory.         In a <code>BUILD</code> file you could do something like this to point to the output path:<br><br>        <pre><code>python         js_binary(             ...             chdir = package_name(),             # ../.. segments to re-relative paths from the chdir back to workspace;             # add an additional 3 segments to account for running js_binary running             # in the root of the output tree             args = ["/".join([".."] * len(package_name().split("/"))) + "$(rootpath //path/to/some:file)"],         )         </code></pre>   | String | optional | <code>""</code> |
| <a id="js_test-copy_data_to_bin"></a>copy_data_to_bin |  When True, <code>data</code> files and the <code>entry_point</code> file are copied to the Bazel output tree before being passed         as inputs to runfiles.<br><br>        Defaults to True so that a <code>js_binary</code> with the default value is compatible with <code>js_run_binary</code> with         <code>use_execroot_entry_point</code> set to True, the default there.<br><br>        Setting this to False is more optimal in terms of inputs, but there is a yet unresolved issue of ESM imports         skirting the node fs patches and escaping the sandbox: https://github.com/aspect-build/rules_js/issues/362.         This is hit in some popular test runners such as mocha, which use native <code>import()</code> statements         (https://github.com/aspect-build/rules_js/pull/353). When set to False, a program such as mocha that uses ESM         imports may escape the execroot by following symlinks into the source tree. When set to True, such a program         would escape the sandbox but will end up in the output tree where <code>node_modules</code> and other inputs required         will be available.   | Boolean | optional | <code>True</code> |
| <a id="js_test-data"></a>data |  Runtime dependencies of the program.<br><br>        The transitive closure of the <code>data</code> dependencies will be available in         the .runfiles folder for this binary/test.<br><br>        NB: <code>data</code> files are copied to the Bazel output tree before being passed         as inputs to runfiles. See <code>copy_data_to_bin</code> docstring for more info.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional | <code>[]</code> |
| <a id="js_test-enable_runfiles"></a>enable_runfiles |  Whether runfiles are enabled in the current build configuration.<br><br>        Typical usage of this rule is via a macro which automatically sets this         attribute based on a <code>config_setting</code> rule.   | Boolean | required |  |
| <a id="js_test-entry_point"></a>entry_point |  The main script which is evaluated by node.js.<br><br>        This is the module referenced by the <code>require.main</code> property in the runtime.<br><br>        This must be a target that provides a single file or a <code>DirectoryPathInfo</code>         from <code>@aspect_bazel_lib//lib::directory_path.bzl</code>.<br><br>        See https://github.com/aspect-build/bazel-lib/blob/main/docs/directory_path.md         for more info on creating a target that provides a <code>DirectoryPathInfo</code>.   | <a href="https://bazel.build/concepts/labels">Label</a> | required |  |
| <a id="js_test-env"></a>env |  Environment variables of the action.<br><br>        Subject to [$(location)](https://bazel.build/reference/be/make-variables#predefined_label_variables)         and ["Make variable"](https://bazel.build/reference/be/make-variables) substitution.   | <a href="https://bazel.build/rules/lib/dict">Dictionary: String -> String</a> | optional | <code>{}</code> |
| <a id="js_test-expected_exit_code"></a>expected_exit_code |  The expected exit code.<br><br>        Can be used to write tests that are expected to fail.   | Integer | optional | <code>0</code> |
| <a id="js_test-fixed_args"></a>fixed_args |  Fixed command line arguments to pass to the Node.js when this         binary target is executed.<br><br>        Subject to [$(location)](https://bazel.build/reference/be/make-variables#predefined_label_variables)         and ["Make variable"](https://bazel.build/reference/be/make-variables) substitution.<br><br>        Unlike the built-in <code>args</code>, which are only passed to the target when it is         executed either by the <code>bazel run</code> command or as a test, <code>fixed_args</code> are baked         into the generated launcher script so are always passed even when the binary         target is run outside of Bazel directly from the launcher script.<br><br>        <code>fixed_args</code> are passed before the ones specified in <code>args</code> and before ones         that are specified on the <code>bazel run</code> or <code>bazel test</code> command line.<br><br>        See https://bazel.build/reference/be/common-definitions#common-attributes-binaries         for more info on the built-in <code>args</code> attribute.   | List of strings | optional | <code>[]</code> |
| <a id="js_test-include_declarations"></a>include_declarations |  When True, <code>declarations</code> and <code>transitive_declarations</code> from <code>JsInfo</code> providers in data targets are included in the runfiles of the target.<br><br>        Defaults to false since declarations are generally not needed at runtime and introducing them could slow down developer round trip         time due to having to generate typings on source file changes.   | Boolean | optional | <code>False</code> |
| <a id="js_test-include_npm"></a>include_npm |  When True, npm is included in the runfiles of the target.<br><br>        An npm binary is also added on the PATH so tools can spawn npm processes. This is a bash script         on Linux and MacOS and a batch script on Windows.<br><br>        A minimum of rules_nodejs version 5.7.0 is required which contains the Node.js toolchain changes         to use npm.   | Boolean | optional | <code>False</code> |
| <a id="js_test-include_npm_linked_packages"></a>include_npm_linked_packages |  When True, files in <code>npm_linked_packages</code> and <code>transitive_npm_linked_packages</code> from <code>JsInfo</code> providers in data targets are included in the runfiles of the target.<br><br>        <code>transitive_files</code> from <code>NpmPackageStoreInfo</code> providers in data targets are also included in the runfiles of the target.   | Boolean | optional | <code>True</code> |
| <a id="js_test-include_transitive_sources"></a>include_transitive_sources |  When True, <code>transitive_sources</code> from <code>JsInfo</code> providers in data targets are included in the runfiles of the target.   | Boolean | optional | <code>True</code> |
| <a id="js_test-log_level"></a>log_level |  Set the logging level.<br><br>        Log from are written to stderr. They will be supressed on success when running as the tool         of a js_run_binary when silent_on_success is True. In that case, they will be shown         only on a build failure along with the stdout & stderr of the node tool being run.<br><br>        Log levels: fatal, error, warn, info, debug   | String | optional | <code>"error"</code> |
| <a id="js_test-no_copy_to_bin"></a>no_copy_to_bin |  List of files to not copy to the Bazel output tree when <code>copy_data_to_bin</code> is True.<br><br>        This is useful for exceptional cases where a <code>copy_to_bin</code> is not possible or not suitable for an input         file such as a file in an external repository. In most cases, this option is not needed.         See <code>copy_data_to_bin</code> docstring for more info.   | <a href="https://bazel.build/concepts/labels">List of labels</a> | optional | <code>[]</code> |
| <a id="js_test-node_options"></a>node_options |  Options to pass to the node invocation on the command line.<br><br>        https://nodejs.org/api/cli.html<br><br>        These options are passed directly to the node invocation on the command line.         Options passed here will take precendence over options passed via         the NODE_OPTIONS environment variable. Options passed here are not added         to the NODE_OPTIONS environment variable so will not be automatically         picked up by child processes that inherit that enviroment variable.   | List of strings | optional | <code>[]</code> |
| <a id="js_test-node_toolchain"></a>node_toolchain |  The Node.js toolchain to use for this target.<br><br>        See https://bazelbuild.github.io/rules_nodejs/Toolchains.html<br><br>        Typically this is left unset so that Bazel automatically selects the right Node.js toolchain         for the target platform. See https://bazel.build/extending/toolchains#toolchain-resolution         for more information.   | <a href="https://bazel.build/concepts/labels">Label</a> | optional | <code>None</code> |
| <a id="js_test-patch_node_fs"></a>patch_node_fs |  Patch the to Node.js <code>fs</code> API (https://nodejs.org/api/fs.html) for this node program         to prevent the program from following symlinks out of the execroot, runfiles and the sandbox.<br><br>        When enabled, <code>js_binary</code> patches the Node.js sync and async <code>fs</code> API functions <code>lstat</code>,         <code>readlink</code>, <code>realpath</code>, <code>readdir</code> and <code>opendir</code> so that the node program being         run cannot resolve symlinks out of the execroot and the runfiles tree. When in the sandbox,         these patches prevent the program being run from resolving symlinks out of the sandbox.<br><br>        When disabled, node programs can leave the execroot, runfiles and sandbox by following symlinks         which can lead to non-hermetic behavior.   | Boolean | optional | <code>True</code> |
| <a id="js_test-preserve_symlinks_main"></a>preserve_symlinks_main |  When True, the --preserve-symlinks-main flag is passed to node.<br><br>        This prevents node from following an ESM entry script out of runfiles and the sandbox. This can happen for <code>.mjs</code>         ESM entry points where the fs node patches, which guard the runfiles and sandbox, are not applied.         See https://github.com/aspect-build/rules_js/issues/362 for more information. Once #362 is resolved,         the default for this attribute can be set to False.<br><br>        This flag was added in Node.js v10.2.0 (released 2018-05-23). If your node toolchain is configured to use a         Node.js version older than this you'll need to set this attribute to False.<br><br>        See https://nodejs.org/api/cli.html#--preserve-symlinks-main for more information.   | Boolean | optional | <code>True</code> |
| <a id="js_test-unresolved_symlinks_enabled"></a>unresolved_symlinks_enabled |  Whether unresolved symlinks are enabled in the current build configuration.<br><br>        These are enabled with the <code>--allow_unresolved_symlinks</code> flag         (named <code>--experimental_allow_unresolved_symlinks in Bazel versions prior to 7.0).<br><br>        Typical usage of this rule is via a macro which automatically sets this         attribute based on a </code>config_setting<code> rule.         See /js/private/BUILD.bazel in rules_js for an example.   | Boolean | optional | <code>False</code> |


<a id="js_binary_lib.create_launcher"></a>

## js_binary_lib.create_launcher

<pre>
js_binary_lib.create_launcher(<a href="#js_binary_lib.create_launcher-ctx">ctx</a>, <a href="#js_binary_lib.create_launcher-log_prefix_rule_set">log_prefix_rule_set</a>, <a href="#js_binary_lib.create_launcher-log_prefix_rule">log_prefix_rule</a>, <a href="#js_binary_lib.create_launcher-fixed_args">fixed_args</a>, <a href="#js_binary_lib.create_launcher-fixed_env">fixed_env</a>)
</pre>



**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="js_binary_lib.create_launcher-ctx"></a>ctx |  <p align="center"> - </p>   |  none |
| <a id="js_binary_lib.create_launcher-log_prefix_rule_set"></a>log_prefix_rule_set |  <p align="center"> - </p>   |  none |
| <a id="js_binary_lib.create_launcher-log_prefix_rule"></a>log_prefix_rule |  <p align="center"> - </p>   |  none |
| <a id="js_binary_lib.create_launcher-fixed_args"></a>fixed_args |  <p align="center"> - </p>   |  <code>[]</code> |
| <a id="js_binary_lib.create_launcher-fixed_env"></a>fixed_env |  <p align="center"> - </p>   |  <code>{}</code> |


<a id="js_binary_lib.implementation"></a>

## js_binary_lib.implementation

<pre>
js_binary_lib.implementation(<a href="#js_binary_lib.implementation-ctx">ctx</a>)
</pre>



**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="js_binary_lib.implementation-ctx"></a>ctx |  <p align="center"> - </p>   |  none |


