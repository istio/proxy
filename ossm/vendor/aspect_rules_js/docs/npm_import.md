<!-- Generated with Stardoc: http://skydoc.bazel.build -->


Repository rule to fetch npm packages.

Load this with,

```starlark
load("@aspect_rules_js//npm:repositories.bzl", "npm_import")
```

This uses Bazel's downloader to fetch the packages.
You can use this to redirect all fetches through a store like Artifactory.

See &lt;https://blog.aspect.dev/configuring-bazels-downloader&gt; for more info about how it works
and how to configure it.

See [`npm_translate_lock`](#npm_translate_lock) for the primary user-facing API to fetch npm packages
for a given lockfile.


<a id="npm_import"></a>

## npm_import

<pre>
npm_import(<a href="#npm_import-name">name</a>, <a href="#npm_import-package">package</a>, <a href="#npm_import-version">version</a>, <a href="#npm_import-deps">deps</a>, <a href="#npm_import-extra_build_content">extra_build_content</a>, <a href="#npm_import-transitive_closure">transitive_closure</a>, <a href="#npm_import-root_package">root_package</a>,
           <a href="#npm_import-link_workspace">link_workspace</a>, <a href="#npm_import-link_packages">link_packages</a>, <a href="#npm_import-lifecycle_hooks">lifecycle_hooks</a>, <a href="#npm_import-lifecycle_hooks_execution_requirements">lifecycle_hooks_execution_requirements</a>,
           <a href="#npm_import-lifecycle_hooks_env">lifecycle_hooks_env</a>, <a href="#npm_import-lifecycle_hooks_use_default_shell_env">lifecycle_hooks_use_default_shell_env</a>, <a href="#npm_import-integrity">integrity</a>, <a href="#npm_import-url">url</a>, <a href="#npm_import-commit">commit</a>,
           <a href="#npm_import-replace_package">replace_package</a>, <a href="#npm_import-package_visibility">package_visibility</a>, <a href="#npm_import-patch_args">patch_args</a>, <a href="#npm_import-patches">patches</a>, <a href="#npm_import-custom_postinstall">custom_postinstall</a>, <a href="#npm_import-npm_auth">npm_auth</a>,
           <a href="#npm_import-npm_auth_basic">npm_auth_basic</a>, <a href="#npm_import-npm_auth_username">npm_auth_username</a>, <a href="#npm_import-npm_auth_password">npm_auth_password</a>, <a href="#npm_import-bins">bins</a>, <a href="#npm_import-dev">dev</a>,
           <a href="#npm_import-register_copy_directory_toolchains">register_copy_directory_toolchains</a>, <a href="#npm_import-register_copy_to_directory_toolchains">register_copy_to_directory_toolchains</a>,
           <a href="#npm_import-run_lifecycle_hooks">run_lifecycle_hooks</a>, <a href="#npm_import-lifecycle_hooks_no_sandbox">lifecycle_hooks_no_sandbox</a>, <a href="#npm_import-kwargs">kwargs</a>)
</pre>

Import a single npm package into Bazel.

Normally you'd want to use `npm_translate_lock` to import all your packages at once.
It generates `npm_import` rules.
You can create these manually if you want to have exact control.

Bazel will only fetch the given package from an external registry if the package is
required for the user-requested targets to be build/tested.

This is a repository rule, which should be called from your `WORKSPACE` file
or some `.bzl` file loaded from it. For example, with this code in `WORKSPACE`:

```starlark
npm_import(
    name = "npm__at_types_node__15.12.2",
    package = "@types/node",
    version = "15.12.2",
    integrity = "sha512-zjQ69G564OCIWIOHSXyQEEDpdpGl+G348RAKY0XXy9Z5kU9Vzv1GMNnkar/ZJ8dzXB3COzD9Mo9NtRZ4xfgUww==",
)
```

&gt; This is similar to Bazel rules in other ecosystems named "_import" like
&gt; `apple_bundle_import`, `scala_import`, `java_import`, and `py_import`.
&gt; `go_repository` is also a model for this rule.

The name of this repository should contain the version number, so that multiple versions of the same
package don't collide.
(Note that the npm ecosystem always supports multiple versions of a library depending on where
it is required, unlike other languages like Go or Python.)

To consume the downloaded package in rules, it must be "linked" into the link package in the
package's `BUILD.bazel` file:

```
load("@npm__at_types_node__15.12.2__links//:defs.bzl", npm_link_types_node = "npm_link_imported_package")

npm_link_types_node(name = "node_modules")
```

This links `@types/node` into the `node_modules` of this package with the target name `:node_modules/@types/node`.

A `:node_modules/@types/node/dir` filegroup target is also created that provides the the directory artifact of the npm package.
This target can be used to create entry points for binary target or to access files within the npm package.

NB: You can choose any target name for the link target but we recommend using the `node_modules/@scope/name` and
`node_modules/name` convention for readability.

When using `npm_translate_lock`, you can link all the npm dependencies in the lock file for a package:

```
load("@npm//:defs.bzl", "npm_link_all_packages")

npm_link_all_packages(name = "node_modules")
```

This creates `:node_modules/name` and `:node_modules/@scope/name` targets for all direct npm dependencies in the package.
It also creates `:node_modules/name/dir` and `:node_modules/@scope/name/dir` filegroup targets that provide the the directory artifacts of their npm packages.
These target can be used to create entry points for binary target or to access files within the npm package.

If you have a mix of `npm_link_all_packages` and `npm_link_imported_package` functions to call you can pass the
`npm_link_imported_package` link functions to the `imported_links` attribute of `npm_link_all_packages` to link
them all in one call. For example,

```
load("@npm//:defs.bzl", "npm_link_all_packages")
load("@npm__at_types_node__15.12.2__links//:defs.bzl", npm_link_types_node = "npm_link_imported_package")

npm_link_all_packages(
    name = "node_modules",
    imported_links = [
        npm_link_types_node,
    ]
)
```

This has the added benefit of adding the `imported_links` to the convienence `:node_modules` target which
includes all direct dependencies in that package.

NB: You can pass an name to npm_link_all_packages and this will change the targets generated to "{name}/@scope/name" and
"{name}/name". We recommend using "node_modules" as the convention for readability.

To change the proxy URL we use to fetch, configure the Bazel downloader:

1. Make a file containing a rewrite rule like

    `rewrite (registry.nodejs.org)/(.*) artifactory.build.internal.net/artifactory/$1/$2`

1. To understand the rewrites, see [UrlRewriterConfig] in Bazel sources.

1. Point bazel to the config with a line in .bazelrc like
common --experimental_downloader_config=.bazel_downloader_config

Read more about the downloader config: &lt;https://blog.aspect.dev/configuring-bazels-downloader&gt;

[UrlRewriterConfig]: https://github.com/bazelbuild/bazel/blob/4.2.1/src/main/java/com/google/devtools/build/lib/bazel/repository/downloader/UrlRewriterConfig.java#L66


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="npm_import-name"></a>name |  Name for this repository rule   |  none |
| <a id="npm_import-package"></a>package |  Name of the npm package, such as <code>acorn</code> or <code>@types/node</code>   |  none |
| <a id="npm_import-version"></a>version |  Version of the npm package, such as <code>8.4.0</code>   |  none |
| <a id="npm_import-deps"></a>deps |  A dict other npm packages this one depends on where the key is the package name and value is the version   |  <code>{}</code> |
| <a id="npm_import-extra_build_content"></a>extra_build_content |  Additional content to append on the generated BUILD file at the root of the created repository, either as a string or a list of lines similar to &lt;https://github.com/bazelbuild/bazel-skylib/blob/main/docs/write_file_doc.md&gt;.   |  <code>""</code> |
| <a id="npm_import-transitive_closure"></a>transitive_closure |  A dict all npm packages this one depends on directly or transitively where the key is the package name and value is a list of version(s) depended on in the closure.   |  <code>{}</code> |
| <a id="npm_import-root_package"></a>root_package |  The root package where the node_modules virtual store is linked to. Typically this is the package that the pnpm-lock.yaml file is located when using <code>npm_translate_lock</code>.   |  <code>""</code> |
| <a id="npm_import-link_workspace"></a>link_workspace |  The workspace name where links will be created for this package.<br><br>This is typically set in rule sets and libraries that are to be consumed as external repositories so links are created in the external repository and not the user workspace.<br><br>Can be left unspecified if the link workspace is the user workspace.   |  <code>""</code> |
| <a id="npm_import-link_packages"></a>link_packages |  Dict of paths where links may be created at for this package to a list of link aliases to link as in each package. If aliases are an empty list this indicates to link as the package name.<br><br>Defaults to {} which indicates that links may be created in any package as specified by the <code>direct</code> attribute of the generated npm_link_package.   |  <code>{}</code> |
| <a id="npm_import-lifecycle_hooks"></a>lifecycle_hooks |  List of lifecycle hook <code>package.json</code> scripts to run for this package if they exist.   |  <code>[]</code> |
| <a id="npm_import-lifecycle_hooks_execution_requirements"></a>lifecycle_hooks_execution_requirements |  Execution requirements when running the lifecycle hooks.<br><br>For example:<br><br><pre><code> lifecycle_hooks_execution_requirements: ["no-sandbox', "requires-network"] </code></pre><br><br>This defaults to ["no-sandbox"] to limit the overhead of sandbox creation and copying the output TreeArtifact out of the sandbox.   |  <code>["no-sandbox"]</code> |
| <a id="npm_import-lifecycle_hooks_env"></a>lifecycle_hooks_env |  Environment variables set for the lifecycle hooks action for this npm package if there is one.<br><br>Environment variables are defined by providing an array of "key=value" entries.<br><br>For example:<br><br><pre><code> lifecycle_hooks_env: ["PREBULT_BINARY=https://downloadurl"], </code></pre>   |  <code>[]</code> |
| <a id="npm_import-lifecycle_hooks_use_default_shell_env"></a>lifecycle_hooks_use_default_shell_env |  If True, the <code>use_default_shell_env</code> attribute of lifecycle hook actions is set to True.<br><br>See [use_default_shell_env](https://bazel.build/rules/lib/builtins/actions#run.use_default_shell_env)<br><br>Note: [--incompatible_merge_fixed_and_default_shell_env](https://bazel.build/reference/command-line-reference#flag--incompatible_merge_fixed_and_default_shell_env) is often required and not enabled by default in Bazel &lt; 7.0.0.<br><br>This defaults to False reduce the negative effects of <code>use_default_shell_env</code>. Requires bazel-lib &gt;= 2.4.2.   |  <code>False</code> |
| <a id="npm_import-integrity"></a>integrity |  Expected checksum of the file downloaded, in Subresource Integrity format. This must match the checksum of the file downloaded.<br><br>This is the same as appears in the pnpm-lock.yaml, yarn.lock or package-lock.json file.<br><br>It is a security risk to omit the checksum as remote files can change.<br><br>At best omitting this field will make your build non-hermetic.<br><br>It is optional to make development easier but should be set before shipping.   |  <code>""</code> |
| <a id="npm_import-url"></a>url |  Optional url for this package. If unset, a default npm registry url is generated from the package name and version.<br><br>May start with <code>git+ssh://</code> or <code>git+https://</code> to indicate a git repository. For example,<br><br><pre><code> git+ssh://git@github.com/org/repo.git </code></pre><br><br>If url is configured as a git repository, the commit attribute must be set to the desired commit.   |  <code>""</code> |
| <a id="npm_import-commit"></a>commit |  Specific commit to be checked out if url is a git repository.   |  <code>""</code> |
| <a id="npm_import-replace_package"></a>replace_package |  Use the specified npm_package target when linking instead of the fetched sources for this npm package.<br><br>The injected npm_package target may optionally contribute transitive npm package dependencies on top of the transitive dependencies specified in the pnpm lock file for the same package, however, these transitive dependencies must not collide with pnpm lock specified transitive dependencies.<br><br>Any patches specified for this package will be not applied to the injected npm_package target. They will be applied, however, to the fetches sources so they can still be useful for patching the fetched <code>package.json</code> file, which is used to determine the generated bin entries for the package.<br><br>NB: lifecycle hooks and custom_postinstall scripts, if implicitly or explicitly enabled, will be run on the injected npm_package. These may be disabled explicitly using the <code>lifecycle_hooks</code> attribute.   |  <code>None</code> |
| <a id="npm_import-package_visibility"></a>package_visibility |  Visibility of generated node_module link targets.   |  <code>["//visibility:public"]</code> |
| <a id="npm_import-patch_args"></a>patch_args |  Arguments to pass to the patch tool.<br><br><code>-p1</code> will usually be needed for patches generated by git.   |  <code>["-p0"]</code> |
| <a id="npm_import-patches"></a>patches |  Patch files to apply onto the downloaded npm package.   |  <code>[]</code> |
| <a id="npm_import-custom_postinstall"></a>custom_postinstall |  Custom string postinstall script to run on the installed npm package. Runs after any existing lifecycle hooks if <code>run_lifecycle_hooks</code> is True.   |  <code>""</code> |
| <a id="npm_import-npm_auth"></a>npm_auth |  Auth token to authenticate with npm. When using Bearer authentication.   |  <code>""</code> |
| <a id="npm_import-npm_auth_basic"></a>npm_auth_basic |  Auth token to authenticate with npm. When using Basic authentication.<br><br>This is typically the base64 encoded string "username:password".   |  <code>""</code> |
| <a id="npm_import-npm_auth_username"></a>npm_auth_username |  Auth username to authenticate with npm. When using Basic authentication.   |  <code>""</code> |
| <a id="npm_import-npm_auth_password"></a>npm_auth_password |  Auth password to authenticate with npm. When using Basic authentication.   |  <code>""</code> |
| <a id="npm_import-bins"></a>bins |  Dictionary of <code>node_modules/.bin</code> binary files to create mapped to their node entry points.<br><br>This is typically derived from the "bin" attribute in the package.json file of the npm package being linked.<br><br>For example:<br><br><pre><code> bins = {     "foo": "./foo.js",     "bar": "./bar.js", } </code></pre><br><br>In the future, this field may be automatically populated by npm_translate_lock from information in the pnpm lock file. That feature is currently blocked on https://github.com/pnpm/pnpm/issues/5131.   |  <code>{}</code> |
| <a id="npm_import-dev"></a>dev |  Whether this npm package is a dev dependency   |  <code>False</code> |
| <a id="npm_import-register_copy_directory_toolchains"></a>register_copy_directory_toolchains |  if True, <code>@aspect_bazel_lib//lib:repositories.bzl</code> <code>register_copy_directory_toolchains()</code> is called if the toolchain is not already registered   |  <code>True</code> |
| <a id="npm_import-register_copy_to_directory_toolchains"></a>register_copy_to_directory_toolchains |  if True, <code>@aspect_bazel_lib//lib:repositories.bzl</code> <code>register_copy_to_directory_toolchains()</code> is called if the toolchain is not already registered   |  <code>True</code> |
| <a id="npm_import-run_lifecycle_hooks"></a>run_lifecycle_hooks |  If True, runs <code>preinstall</code>, <code>install</code>, <code>postinstall</code> and 'prepare' lifecycle hooks declared in this package.<br><br>Deprecated. Use <code>lifecycle_hooks</code> instead.   |  <code>None</code> |
| <a id="npm_import-lifecycle_hooks_no_sandbox"></a>lifecycle_hooks_no_sandbox |  If True, adds "no-sandbox" to <code>lifecycle_hooks_execution_requirements</code>.<br><br>Deprecated. Add "no-sandbox" to <code>lifecycle_hooks_execution_requirements</code> instead.   |  <code>None</code> |
| <a id="npm_import-kwargs"></a>kwargs |  Internal use only   |  none |


