<!-- Generated with Stardoc: http://skydoc.bazel.build -->


Rules for linking npm dependencies and packaging and linking first-party deps.

Load these with,

```starlark
load("@aspect_rules_js//npm:defs.bzl", "npm_package")
```


<a id="npm_package"></a>

## npm_package

<pre>
npm_package(<a href="#npm_package-name">name</a>, <a href="#npm_package-srcs">srcs</a>, <a href="#npm_package-data">data</a>, <a href="#npm_package-args">args</a>, <a href="#npm_package-out">out</a>, <a href="#npm_package-package">package</a>, <a href="#npm_package-version">version</a>, <a href="#npm_package-root_paths">root_paths</a>,
            <a href="#npm_package-include_external_repositories">include_external_repositories</a>, <a href="#npm_package-include_srcs_packages">include_srcs_packages</a>, <a href="#npm_package-exclude_srcs_packages">exclude_srcs_packages</a>,
            <a href="#npm_package-include_srcs_patterns">include_srcs_patterns</a>, <a href="#npm_package-exclude_srcs_patterns">exclude_srcs_patterns</a>, <a href="#npm_package-replace_prefixes">replace_prefixes</a>, <a href="#npm_package-allow_overwrites">allow_overwrites</a>,
            <a href="#npm_package-include_sources">include_sources</a>, <a href="#npm_package-include_transitive_sources">include_transitive_sources</a>, <a href="#npm_package-include_declarations">include_declarations</a>,
            <a href="#npm_package-include_transitive_declarations">include_transitive_declarations</a>, <a href="#npm_package-include_runfiles">include_runfiles</a>, <a href="#npm_package-hardlink">hardlink</a>, <a href="#npm_package-publishable">publishable</a>, <a href="#npm_package-verbose">verbose</a>, <a href="#npm_package-kwargs">kwargs</a>)
</pre>

A macro that packages sources into a directory (a tree artifact) and provides an `NpmPackageInfo`.

This target can be used as the `src` attribute to `npm_link_package`.

The macro also produces a target `[name].publish`, that can be run to publish to an npm registry.
Under the hood, this target runs `npm publish`. You can pass arguments to npm by escaping them from Bazel using a double-hyphen,
for example: `bazel run //path/to:my_package.publish -- --tag=next`

Files and directories can be arranged as needed in the output directory using
the `root_paths`, `include_srcs_patterns`, `exclude_srcs_patterns` and `replace_prefixes` attributes.

Filters and transformations are applied in the following order:

1. `include_external_repositories`

2. `include_srcs_packages`

3. `exclude_srcs_packages`

4. `root_paths`

5. `include_srcs_patterns`

6. `exclude_srcs_patterns`

7. `replace_prefixes`

For more information each filters / transformations applied, see
the documentation for the specific filter / transformation attribute.

Glob patterns are supported. Standard wildcards (globbing patterns) plus the `**` doublestar (aka. super-asterisk)
are supported with the underlying globbing library, https://github.com/bmatcuk/doublestar. This is the same
globbing library used by [gazelle](https://github.com/bazelbuild/bazel-gazelle). See https://github.com/bmatcuk/doublestar#patterns
for more information on supported globbing patterns.

`npm_package` makes use of `copy_to_directory`
(https://docs.aspect.build/rules/aspect_bazel_lib/docs/copy_to_directory) under the hood,
adopting its API and its copy action using composition. However, unlike `copy_to_directory`,
`npm_package` includes `transitive_sources` and `transitive_declarations` files from `JsInfo` providers in srcs
by default. The behavior of including sources and declarations from `JsInfo` can be configured
using the `include_sources`, `include_transitive_sources`, `include_declarations`, `include_transitive_declarations`
attributes.

The two `include*_declarations` options may cause type-check actions to run, which slows down your
development round-trip.
You can pass the Bazel option `--@aspect_rules_js//npm:exclude_declarations_from_npm_packages`
to override these two attributes for an individual `bazel` invocation, avoiding the type-check.

`npm_package` also includes default runfiles from `srcs` by default which `copy_to_directory` does not. This behavior
can be configured with the `include_runfiles` attribute.

The default `include_srcs_packages`, `[".", "./**"]`, prevents files from outside of the target's
package and subpackages from being included.

The default `exclude_srcs_patterns`, of `["node_modules/**", "**/node_modules/**"]`, prevents
`node_modules` files from being included.

To stamp the current git tag as the "version" in the package.json file, see
[stamped_package_json](#stamped_package_json)


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="npm_package-name"></a>name |  Unique name for this target.   |  none |
| <a id="npm_package-srcs"></a>srcs |  Files and/or directories or targets that provide <code>DirectoryPathInfo</code> to copy into the output directory.   |  <code>[]</code> |
| <a id="npm_package-data"></a>data |  Runtime / linktime npm dependencies of this npm package.<br><br><code>NpmPackageStoreInfo</code> providers are gathered from <code>JsInfo</code> of the targets specified. Targets can be linked npm packages, npm package store targets or other targets that provide <code>JsInfo</code>. This is done directly from the <code>npm_package_store_deps</code> field of these. For linked npm package targets, the underlying npm_package_store target(s) that back the links is used.<br><br>Gathered <code>NpmPackageStoreInfo</code> providers are used downstream as direct dependencies of this npm package when linking with <code>npm_link_package</code>.   |  <code>[]</code> |
| <a id="npm_package-args"></a>args |  Arguments that are passed down to <code>&lt;name&gt;.publish</code> target and <code>npm publish</code> command.   |  <code>[]</code> |
| <a id="npm_package-out"></a>out |  Path of the output directory, relative to this package.   |  <code>None</code> |
| <a id="npm_package-package"></a>package |  The package name. If set, should match the <code>name</code> field in the <code>package.json</code> file for this package.<br><br>If set, the package name set here will be used for linking if a npm_link_package does not specify a package name. A npm_link_package that specifies a package name will override the value here when linking.<br><br>If unset, a npm_link_package that references this npm_package must define the package name must be for linking.   |  <code>""</code> |
| <a id="npm_package-version"></a>version |  The package version. If set, should match the <code>version</code> field in the <code>package.json</code> file for this package.<br><br>If set, a npm_link_package may omit the package version and the package version set here will be used for linking. A npm_link_package that specifies a package version will override the value here when linking.<br><br>If unset, a npm_link_package that references this npm_package must define the package version must be for linking.   |  <code>"0.0.0"</code> |
| <a id="npm_package-root_paths"></a>root_paths |  List of paths (with glob support) that are roots in the output directory.<br><br>If any parent directory of a file being copied matches one of the root paths patterns specified, the output directory path will be the path relative to the root path instead of the path relative to the file's workspace. If there are multiple root paths that match, the longest match wins.<br><br>Matching is done on the parent directory of the output file path so a trailing '**' glob patterm will match only up to the last path segment of the dirname and will not include the basename. Only complete path segments are matched. Partial matches on the last segment of the root path are ignored.<br><br>Forward slashes (<code>/</code>) should be used as path separators.<br><br>A <code>"."</code> value expands to the target's package path (<code>ctx.label.package</code>).<br><br>Defaults to <code>["."]</code> which results in the output directory path of files in the target's package and and sub-packages are relative to the target's package and files outside of that retain their full workspace relative paths.<br><br>Globs are supported (see rule docstring above).   |  <code>["."]</code> |
| <a id="npm_package-include_external_repositories"></a>include_external_repositories |  List of external repository names (with glob support) to include in the output directory.<br><br>Files from external repositories are only copied into the output directory if the external repository they come from matches one of the external repository patterns specified.<br><br>When copied from an external repository, the file path in the output directory defaults to the file's path within the external repository. The external repository name is _not_ included in that path.<br><br>For example, the following copies <code>@external_repo//path/to:file</code> to <code>path/to/file</code> within the output directory.<br><br><pre><code> npp_package(     name = "dir",     include_external_repositories = ["external_*"],     srcs = ["@external_repo//path/to:file"], ) </code></pre><br><br>Files that come from matching external are subject to subsequent filters and transformations to determine if they are copied and what their path in the output directory will be. The external repository name of the file from an external repository is not included in the output directory path and is considered in subsequent filters and transformations.<br><br>Globs are supported (see rule docstring above).   |  <code>[]</code> |
| <a id="npm_package-include_srcs_packages"></a>include_srcs_packages |  List of Bazel packages (with glob support) to include in output directory.<br><br>Files in srcs are only copied to the output directory if the Bazel package of the file matches one of the patterns specified.<br><br>Forward slashes (<code>/</code>) should be used as path separators. A first character of <code>"."</code> will be replaced by the target's package path.<br><br>Defaults to ["./**"] which includes sources target's package and subpackages.<br><br>Files that have matching Bazel packages are subject to subsequent filters and transformations to determine if they are copied and what their path in the output directory will be.<br><br>Globs are supported (see rule docstring above).   |  <code>["./**"]</code> |
| <a id="npm_package-exclude_srcs_packages"></a>exclude_srcs_packages |  List of Bazel packages (with glob support) to exclude from output directory.<br><br>Files in srcs are not copied to the output directory if the Bazel package of the file matches one of the patterns specified.<br><br>Forward slashes (<code>/</code>) should be used as path separators. A first character of <code>"."</code> will be replaced by the target's package path.<br><br>Defaults to ["**/node_modules/**"] which excludes all node_modules folders from the output directory.<br><br>Files that have do not have matching Bazel packages are subject to subsequent filters and transformations to determine if they are copied and what their path in the output directory will be.<br><br>Globs are supported (see rule docstring above).   |  <code>[]</code> |
| <a id="npm_package-include_srcs_patterns"></a>include_srcs_patterns |  List of paths (with glob support) to include in output directory.<br><br>Files in srcs are only copied to the output directory if their output directory path, after applying <code>root_paths</code>, matches one of the patterns specified.<br><br>Forward slashes (<code>/</code>) should be used as path separators.<br><br>Defaults to <code>["**"]</code> which includes all sources.<br><br>Files that have matching output directory paths are subject to subsequent filters and transformations to determine if they are copied and what their path in the output directory will be.<br><br>Globs are supported (see rule docstring above).   |  <code>["**"]</code> |
| <a id="npm_package-exclude_srcs_patterns"></a>exclude_srcs_patterns |  List of paths (with glob support) to exclude from output directory.<br><br>Files in srcs are not copied to the output directory if their output directory path, after applying <code>root_paths</code>, matches one of the patterns specified.<br><br>Forward slashes (<code>/</code>) should be used as path separators.<br><br>Files that do not have matching output directory paths are subject to subsequent filters and transformations to determine if they are copied and what their path in the output directory will be.<br><br>Globs are supported (see rule docstring above).   |  <code>["**/node_modules/**"]</code> |
| <a id="npm_package-replace_prefixes"></a>replace_prefixes |  Map of paths prefixes (with glob support) to replace in the output directory path when copying files.<br><br>If the output directory path for a file starts with or fully matches a a key in the dict then the matching portion of the output directory path is replaced with the dict value for that key. The final path segment matched can be a partial match of that segment and only the matching portion will be replaced. If there are multiple keys that match, the longest match wins.<br><br>Forward slashes (<code>/</code>) should be used as path separators.<br><br>Replace prefix transformation are the final step in the list of filters and transformations. The final output path of a file being copied into the output directory is determined at this step.<br><br>Globs are supported (see rule docstring above).   |  <code>{}</code> |
| <a id="npm_package-allow_overwrites"></a>allow_overwrites |  If True, allow files to be overwritten if the same output file is copied to twice.<br><br>The order of srcs matters as the last copy of a particular file will win when overwriting. Performance of <code>npm_package</code> will be slightly degraded when allow_overwrites is True since copies cannot be parallelized out as they are calculated. Instead all copy paths must be calculated before any copies can be started.   |  <code>False</code> |
| <a id="npm_package-include_sources"></a>include_sources |  When True, <code>sources</code> from <code>JsInfo</code> providers in data targets are included in the list of available files to copy.   |  <code>True</code> |
| <a id="npm_package-include_transitive_sources"></a>include_transitive_sources |  When True, <code>transitive_sources</code> from <code>JsInfo</code> providers in data targets are included in the list of available files to copy.   |  <code>True</code> |
| <a id="npm_package-include_declarations"></a>include_declarations |  When True, <code>declarations</code> from <code>JsInfo</code> providers in data targets are included in the list of available files to copy.   |  <code>True</code> |
| <a id="npm_package-include_transitive_declarations"></a>include_transitive_declarations |  When True, <code>transitive_declarations</code> from <code>JsInfo</code> providers in data targets are included in the list of available files to copy.   |  <code>True</code> |
| <a id="npm_package-include_runfiles"></a>include_runfiles |  When True, default runfiles from <code>srcs</code> targets are included in the list of available files to copy.<br><br>This may be needed in a few cases:<br><br>- to work-around issues with rules that don't provide everything needed in sources, transitive_sources, declarations & transitive_declarations - to depend on the runfiles targets that don't use JsInfo<br><br>NB: The default value will be flipped to False in the next major release as runfiles are not needed in the general case and adding them to the list of files available to copy can add noticeable overhead to the analysis phase in a large repository with many npm_package targets.   |  <code>True</code> |
| <a id="npm_package-hardlink"></a>hardlink |  Controls when to use hardlinks to files instead of making copies.<br><br>Creating hardlinks is much faster than making copies of files with the caveat that hardlinks share file permissions with their source.<br><br>Since Bazel removes write permissions on files in the output tree after an action completes, hardlinks to source files are not recommended since write permissions will be inadvertently removed from sources files.<br><br>- <code>auto</code>: hardlinks are used for generated files already in the output tree - <code>off</code>: all files are copied - <code>on</code>: hardlinks are used for all files (not recommended)   |  <code>"auto"</code> |
| <a id="npm_package-publishable"></a>publishable |  When True, enable generation of <code>{name}.publish</code> target   |  <code>True</code> |
| <a id="npm_package-verbose"></a>verbose |  If true, prints out verbose logs to stdout   |  <code>False</code> |
| <a id="npm_package-kwargs"></a>kwargs |  Additional attributes such as <code>tags</code> and <code>visibility</code>   |  none |


<a id="npm_package_lib.implementation"></a>

## npm_package_lib.implementation

<pre>
npm_package_lib.implementation(<a href="#npm_package_lib.implementation-ctx">ctx</a>)
</pre>



**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="npm_package_lib.implementation-ctx"></a>ctx |  <p align="center"> - </p>   |  none |


<a id="stamped_package_json"></a>

## stamped_package_json

<pre>
stamped_package_json(<a href="#stamped_package_json-name">name</a>, <a href="#stamped_package_json-stamp_var">stamp_var</a>, <a href="#stamped_package_json-kwargs">kwargs</a>)
</pre>

Convenience wrapper to set the "version" property in package.json with the git tag.

In unstamped builds (typically those without `--stamp`) the version will be set to `0.0.0`.
This ensures that actions which use the package.json file can get cache hits.

For more information on stamping, read https://docs.aspect.build/rules/aspect_bazel_lib/docs/stamping.

Using this rule requires that you register the jq toolchain in your WORKSPACE:

```starlark
load("@aspect_bazel_lib//lib:repositories.bzl", "register_jq_toolchains")

register_jq_toolchains()
```


**PARAMETERS**


| Name  | Description | Default Value |
| :------------- | :------------- | :------------- |
| <a id="stamped_package_json-name"></a>name |  name of the resulting <code>jq</code> target, must be "package"   |  none |
| <a id="stamped_package_json-stamp_var"></a>stamp_var |  a key from the bazel-out/stable-status.txt or bazel-out/volatile-status.txt files   |  none |
| <a id="stamped_package_json-kwargs"></a>kwargs |  additional attributes passed to the jq rule, see https://docs.aspect.build/rules/aspect_bazel_lib/docs/jq   |  none |


