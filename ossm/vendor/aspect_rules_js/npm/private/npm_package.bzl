"""
Rules for linking npm dependencies and packaging and linking first-party deps.

Load these with,

```starlark
load("@aspect_rules_js//npm:defs.bzl", "npm_package")
```
"""

load("@aspect_bazel_lib//lib:copy_to_directory.bzl", "copy_to_directory_bin_action", "copy_to_directory_lib")
load("@aspect_bazel_lib//lib:directory_path.bzl", "DirectoryPathInfo")
load("@aspect_bazel_lib//lib:jq.bzl", "jq")
load("@aspect_bazel_lib//tools:version.bzl", BAZEL_LIB_VERSION = "VERSION")
load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("@bazel_skylib//lib:versions.bzl", "versions")
load("//js:defs.bzl", "js_binary")
load("//js:libs.bzl", "js_lib_helpers")
load("//js:providers.bzl", "JsInfo")
load(":npm_package_info.bzl", "NpmPackageInfo")

# Pull in all copy_to_directory attributes except for exclude_prefixes
copy_to_directory_lib_attrs = dict(copy_to_directory_lib.attrs)
copy_to_directory_lib_attrs.pop(
    "exclude_prefixes",
    # It was removed in bazel-lib 2.0, so default to ignoring.
    None,
)

_NPM_PACKAGE_ATTRS = dicts.add(copy_to_directory_lib_attrs, {
    "data": attr.label_list(),
    "package": attr.string(),
    "version": attr.string(),
})

_NPM_PACKAGE_FILES_ATTRS = {
    "include_declarations": attr.bool(),
    "include_runfiles": attr.bool(),
    "include_sources": attr.bool(),
    "include_transitive_declarations": attr.bool(),
    "include_transitive_sources": attr.bool(),
    "srcs": attr.label_list(allow_files = True),
}

def _npm_package_files_impl(ctx):
    files_depsets = []

    if ctx.attr.include_transitive_sources:
        # include all transitive sources (this includes direct sources)
        files_depsets.extend([
            target[JsInfo].transitive_sources
            for target in ctx.attr.srcs
            if JsInfo in target and hasattr(target[JsInfo], "transitive_sources")
        ])
    elif ctx.attr.include_sources:
        # include only direct sources
        files_depsets.extend([
            target[JsInfo].sources
            for target in ctx.attr.srcs
            if JsInfo in target and hasattr(target[JsInfo], "sources")
        ])

    if ctx.attr.include_transitive_declarations:
        # include all transitive declarations (this includes direct declarations)
        files_depsets.extend([
            target[JsInfo].transitive_declarations
            for target in ctx.attr.srcs
            if JsInfo in target and hasattr(target[JsInfo], "transitive_declarations")
        ])
    elif ctx.attr.include_declarations:
        # include only direct declarations
        files_depsets.extend([
            target[JsInfo].declarations
            for target in ctx.attr.srcs
            if JsInfo in target and hasattr(target[JsInfo], "declarations")
        ])

    if ctx.attr.include_runfiles:
        # include default runfiles from srcs
        files_depsets.extend([
            target[DefaultInfo].default_runfiles.files
            for target in ctx.attr.srcs
        ])

    return DefaultInfo(
        files = depset(transitive = files_depsets),
    )

def _npm_package_impl(ctx):
    dst = ctx.actions.declare_directory(ctx.attr.out if ctx.attr.out else ctx.attr.name)

    # forward all npm_package_store_deps
    npm_package_store_deps = [
        target[JsInfo].npm_package_store_deps
        for target in ctx.attr.srcs
        if JsInfo in target and hasattr(target[JsInfo], "npm_package_store_deps")
    ]
    npm_package_store_deps.append(js_lib_helpers.gather_npm_package_store_deps(
        targets = ctx.attr.data,
    ))

    copy_to_directory_bin_action(
        ctx,
        name = ctx.attr.name,
        copy_to_directory_bin = ctx.toolchains["@aspect_bazel_lib//lib:copy_to_directory_toolchain_type"].copy_to_directory_info.bin,
        dst = dst,
        files = ctx.files.srcs,
        targets = [t for t in ctx.attr.srcs if DirectoryPathInfo in t],
        root_paths = ctx.attr.root_paths,
        include_external_repositories = ctx.attr.include_external_repositories,
        include_srcs_packages = ctx.attr.include_srcs_packages,
        exclude_srcs_packages = ctx.attr.exclude_srcs_packages,
        include_srcs_patterns = ctx.attr.include_srcs_patterns,
        exclude_srcs_patterns = ctx.attr.exclude_srcs_patterns,
        replace_prefixes = ctx.attr.replace_prefixes,
        allow_overwrites = ctx.attr.allow_overwrites,
        hardlink = ctx.attr.hardlink,
        verbose = ctx.attr.verbose,
    )

    # TODO: add a verification action that checks that the package and version match the contained package.json;
    #       if no package.json is found in the directory then optional generate one

    return [
        DefaultInfo(
            files = depset([dst]),
        ),
        NpmPackageInfo(
            package = ctx.attr.package,
            version = ctx.attr.version,
            directory = dst,
            npm_package_store_deps = depset([], transitive = npm_package_store_deps),
        ),
    ]

npm_package_lib = struct(
    attrs = _NPM_PACKAGE_ATTRS,
    implementation = _npm_package_impl,
    provides = [DefaultInfo, NpmPackageInfo],
)

_npm_package = rule(
    implementation = npm_package_lib.implementation,
    attrs = npm_package_lib.attrs,
    provides = npm_package_lib.provides,
    toolchains = [Label("@aspect_bazel_lib//lib:copy_to_directory_toolchain_type")],
)

_npm_package_files = rule(
    implementation = _npm_package_files_impl,
    attrs = _NPM_PACKAGE_FILES_ATTRS,
)

def npm_package(
        name,
        srcs = [],
        data = [],
        args = [],
        out = None,
        package = "",
        version = "0.0.0",
        root_paths = ["."],
        include_external_repositories = [],
        include_srcs_packages = ["./**"],
        exclude_srcs_packages = [],
        include_srcs_patterns = ["**"],
        exclude_srcs_patterns = ["**/node_modules/**"],
        replace_prefixes = {},
        allow_overwrites = False,
        include_sources = True,
        include_transitive_sources = True,
        include_declarations = True,
        include_transitive_declarations = True,
        # TODO(2.0): flip include_runfiles default to False
        include_runfiles = True,
        hardlink = "auto",
        publishable = True,
        verbose = False,
        **kwargs):
    """A macro that packages sources into a directory (a tree artifact) and provides an `NpmPackageInfo`.

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

    Args:
        name: Unique name for this target.

        srcs: Files and/or directories or targets that provide `DirectoryPathInfo` to copy into the output directory.

        args: Arguments that are passed down to `<name>.publish` target and `npm publish` command.

        data: Runtime / linktime npm dependencies of this npm package.

            `NpmPackageStoreInfo` providers are gathered from `JsInfo` of the targets specified. Targets can be linked npm
            packages, npm package store targets or other targets that provide `JsInfo`. This is done directly from the
            `npm_package_store_deps` field of these. For linked npm package targets, the underlying npm_package_store
            target(s) that back the links is used.

            Gathered `NpmPackageStoreInfo` providers are used downstream as direct dependencies of this npm package when
            linking with `npm_link_package`.

        out: Path of the output directory, relative to this package.

        package: The package name. If set, should match the `name` field in the `package.json` file for this package.

            If set, the package name set here will be used for linking if a npm_link_package does not specify a package name. A
            npm_link_package that specifies a package name will override the value here when linking.

            If unset, a npm_link_package that references this npm_package must define the package name must be for linking.

        version: The package version. If set, should match the `version` field in the `package.json` file for this package.

            If set, a npm_link_package may omit the package version and the package version set here will be used for linking. A
            npm_link_package that specifies a package version will override the value here when linking.

            If unset, a npm_link_package that references this npm_package must define the package version must be for linking.

        root_paths: List of paths (with glob support) that are roots in the output directory.

            If any parent directory of a file being copied matches one of the root paths
            patterns specified, the output directory path will be the path relative to the root path
            instead of the path relative to the file's workspace. If there are multiple
            root paths that match, the longest match wins.

            Matching is done on the parent directory of the output file path so a trailing '**' glob patterm
            will match only up to the last path segment of the dirname and will not include the basename.
            Only complete path segments are matched. Partial matches on the last segment of the root path
            are ignored.

            Forward slashes (`/`) should be used as path separators.

            A `"."` value expands to the target's package path (`ctx.label.package`).

            Defaults to `["."]` which results in the output directory path of files in the
            target's package and and sub-packages are relative to the target's package and
            files outside of that retain their full workspace relative paths.

            Globs are supported (see rule docstring above).

        include_external_repositories: List of external repository names (with glob support) to include in the output directory.

            Files from external repositories are only copied into the output directory if
            the external repository they come from matches one of the external repository patterns
            specified.

            When copied from an external repository, the file path in the output directory
            defaults to the file's path within the external repository. The external repository
            name is _not_ included in that path.

            For example, the following copies `@external_repo//path/to:file` to
            `path/to/file` within the output directory.

            ```
            npp_package(
                name = "dir",
                include_external_repositories = ["external_*"],
                srcs = ["@external_repo//path/to:file"],
            )
            ```

            Files that come from matching external are subject to subsequent filters and
            transformations to determine if they are copied and what their path in the output
            directory will be. The external repository name of the file from an external
            repository is not included in the output directory path and is considered in subsequent
            filters and transformations.

            Globs are supported (see rule docstring above).

        include_srcs_packages: List of Bazel packages (with glob support) to include in output directory.

            Files in srcs are only copied to the output directory if
            the Bazel package of the file matches one of the patterns specified.

            Forward slashes (`/`) should be used as path separators. A first character of `"."`
            will be replaced by the target's package path.

            Defaults to ["./**"] which includes sources target's package and subpackages.

            Files that have matching Bazel packages are subject to subsequent filters and
            transformations to determine if they are copied and what their path in the output
            directory will be.

            Globs are supported (see rule docstring above).

        exclude_srcs_packages: List of Bazel packages (with glob support) to exclude from output directory.

            Files in srcs are not copied to the output directory if
            the Bazel package of the file matches one of the patterns specified.

            Forward slashes (`/`) should be used as path separators. A first character of `"."`
            will be replaced by the target's package path.

            Defaults to ["**/node_modules/**"] which excludes all node_modules folders
            from the output directory.

            Files that have do not have matching Bazel packages are subject to subsequent
            filters and transformations to determine if they are copied and what their path in the output
            directory will be.

            Globs are supported (see rule docstring above).

        include_srcs_patterns: List of paths (with glob support) to include in output directory.

            Files in srcs are only copied to the output directory if their output
            directory path, after applying `root_paths`, matches one of the patterns specified.

            Forward slashes (`/`) should be used as path separators.

            Defaults to `["**"]` which includes all sources.

            Files that have matching output directory paths are subject to subsequent
            filters and transformations to determine if they are copied and what their path in the output
            directory will be.

            Globs are supported (see rule docstring above).

        exclude_srcs_patterns: List of paths (with glob support) to exclude from output directory.

            Files in srcs are not copied to the output directory if their output
            directory path, after applying `root_paths`, matches one of the patterns specified.

            Forward slashes (`/`) should be used as path separators.

            Files that do not have matching output directory paths are subject to subsequent
            filters and transformations to determine if they are copied and what their path in the output
            directory will be.

            Globs are supported (see rule docstring above).

        replace_prefixes: Map of paths prefixes (with glob support) to replace in the output directory path when copying files.

            If the output directory path for a file starts with or fully matches a
            a key in the dict then the matching portion of the output directory path is
            replaced with the dict value for that key. The final path segment
            matched can be a partial match of that segment and only the matching portion will
            be replaced. If there are multiple keys that match, the longest match wins.

            Forward slashes (`/`) should be used as path separators.

            Replace prefix transformation are the final step in the list of filters and transformations.
            The final output path of a file being copied into the output directory
            is determined at this step.

            Globs are supported (see rule docstring above).

        allow_overwrites: If True, allow files to be overwritten if the same output file is copied to twice.

            The order of srcs matters as the last copy of a particular file will win when overwriting.
            Performance of `npm_package` will be slightly degraded when allow_overwrites is True
            since copies cannot be parallelized out as they are calculated. Instead all copy paths
            must be calculated before any copies can be started.

        include_sources: When True, `sources` from `JsInfo` providers in data targets are included in the list of available files to copy.

        include_transitive_sources: When True, `transitive_sources` from `JsInfo` providers in data targets are included in the list of available files to copy.

        include_declarations: When True, `declarations` from `JsInfo` providers in data targets are included in the list of available files to copy.

        include_transitive_declarations: When True, `transitive_declarations` from `JsInfo` providers in data targets are included in the list of available files to copy.

        include_runfiles: When True, default runfiles from `srcs` targets are included in the list of available files to copy.

            This may be needed in a few cases:

            - to work-around issues with rules that don't provide everything needed in sources, transitive_sources, declarations & transitive_declarations
            - to depend on the runfiles targets that don't use JsInfo

            NB: The default value will be flipped to False in the next major release as runfiles are not needed in the general case
            and adding them to the list of files available to copy can add noticeable overhead to the analysis phase in a large
            repository with many npm_package targets.

        hardlink: Controls when to use hardlinks to files instead of making copies.

            Creating hardlinks is much faster than making copies of files with the caveat that
            hardlinks share file permissions with their source.

            Since Bazel removes write permissions on files in the output tree after an action completes,
            hardlinks to source files are not recommended since write permissions will be inadvertently
            removed from sources files.

            - `auto`: hardlinks are used for generated files already in the output tree
            - `off`: all files are copied
            - `on`: hardlinks are used for all files (not recommended)

        publishable: When True, enable generation of `{name}.publish` target

        verbose: If true, prints out verbose logs to stdout

        **kwargs: Additional attributes such as `tags` and `visibility`
    """

    if include_sources or include_transitive_sources or include_declarations or include_transitive_declarations or include_runfiles:
        files_target = "{}_files".format(name)
        _npm_package_files(
            name = files_target,
            srcs = srcs,
            include_sources = include_sources,
            include_transitive_sources = include_transitive_sources,
            include_declarations = select({
                Label("@aspect_rules_js//npm:exclude_declarations_from_npm_packages_flag"): False,
                "//conditions:default": include_declarations,
            }),
            include_transitive_declarations = select({
                Label("@aspect_rules_js//npm:exclude_declarations_from_npm_packages_flag"): False,
                "//conditions:default": include_transitive_declarations,
            }),
            include_runfiles = include_runfiles,
            # Always tag the target manual since we should only build it when the final target is built.
            tags = kwargs.get("tags", []) + ["manual"],
            # Always propagate the testonly attribute
            testonly = kwargs.get("testonly", False),
        )
        srcs = srcs + [files_target]

    # TODO(2.0): switch to false
    if publishable:
        js_binary(
            name = "{}.publish".format(name),
            entry_point = Label("@aspect_rules_js//npm/private:npm_publish_mjs"),
            fixed_args = [
                "$(rootpath :{})".format(name),
            ],
            data = [name],
            # required to make npm to be available in PATH
            include_npm = True,
            args = args,
            tags = kwargs.get("tags", []) + ["manual"],
            testonly = kwargs.get("testonly", False),
            visibility = kwargs.get("visibility", None),
        )

    _npm_package(
        name = name,
        srcs = srcs,
        data = data,
        out = out,
        package = package,
        version = version,
        root_paths = root_paths,
        include_external_repositories = include_external_repositories,
        include_srcs_packages = include_srcs_packages,
        exclude_srcs_packages = exclude_srcs_packages,
        include_srcs_patterns = include_srcs_patterns,
        exclude_srcs_patterns = exclude_srcs_patterns,
        replace_prefixes = replace_prefixes,
        allow_overwrites = allow_overwrites,
        hardlink = hardlink,
        verbose = verbose,
        **kwargs
    )

def stamped_package_json(name, stamp_var, **kwargs):
    """Convenience wrapper to set the "version" property in package.json with the git tag.

    In unstamped builds (typically those without `--stamp`) the version will be set to `0.0.0`.
    This ensures that actions which use the package.json file can get cache hits.

    For more information on stamping, read https://docs.aspect.build/rules/aspect_bazel_lib/docs/stamping.

    Using this rule requires that you register the jq toolchain in your WORKSPACE:

    ```starlark
    load("@aspect_bazel_lib//lib:repositories.bzl", "register_jq_toolchains")

    register_jq_toolchains()
    ```

    Args:
        name: name of the resulting `jq` target, must be "package"
        stamp_var: a key from the bazel-out/stable-status.txt or bazel-out/volatile-status.txt files
        **kwargs: additional attributes passed to the jq rule, see https://docs.aspect.build/rules/aspect_bazel_lib/docs/jq
    """
    if name != "package":
        fail("""stamped_package_json should always be named "package" so that the default output is named "package.json".
        This is required since Bazel doesn't allow a predeclared output to have the same name as an input file.""")

    jq(
        name = name,
        srcs = ["package.json"],
        filter = "|".join([
            # Don't directly reference $STAMP as it's only set when stamping
            # This 'as' syntax results in $stamp being null in unstamped builds.
            "$ARGS.named.STAMP as $stamp",
            # Provide a default using the "alternative operator" in case $stamp is null.
            ".version = ($stamp{}.{} // \"0.0.0\")".format(
                # bazel-lib 1/2 require different syntax
                "[0]" if versions.is_at_least("2.0.0", BAZEL_LIB_VERSION) else "",
                stamp_var,
            ),
        ]),
        **kwargs
    )
