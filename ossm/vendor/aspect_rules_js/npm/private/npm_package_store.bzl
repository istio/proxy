"npm_package_store rule"

load("@aspect_bazel_lib//lib:copy_directory.bzl", "copy_directory_bin_action")
load("@bazel_skylib//lib:paths.bzl", "paths")
load(":utils.bzl", "utils")
load(":npm_package_info.bzl", "NpmPackageInfo")
load(":npm_package_store_info.bzl", "NpmPackageStoreInfo")

_DOC = """Defines a npm package that is linked into a node_modules tree.

The npm package is linked with a pnpm style symlinked node_modules output tree.

The term "package" is defined at
<https://nodejs.org/docs/latest-v16.x/api/packages.html>

See https://pnpm.io/symlinked-node-modules-structure for more information on
the symlinked node_modules structure.
Npm may also support a symlinked node_modules structure called
"Isolated mode" in the future:
https://github.com/npm/rfcs/blob/main/accepted/0042-isolated-mode.md.
"""

_ATTRS = {
    "src": attr.label(
        doc = """A npm_package target or or any other target that provides a NpmPackageInfo.
        """,
        providers = [NpmPackageInfo],
        mandatory = True,
    ),
    "deps": attr.label_keyed_string_dict(
        doc = """Other node packages store link targets one depends on mapped to the name to link them under in this packages deps.

        This should include *all* modules the program may need at runtime.

        You can find all the package store link targets in your repository with

        ```
        bazel query ... | grep :.aspect_rules_js | grep -v /dir | grep -v /pkg | grep -v /ref
        ```

        Package store link targets names for 3rd party packages that come from `npm_translate_lock`
        start with `.aspect_rules_js/` then the name passed to the `npm_link_all_packages` macro
        (typically `node_modules`) followed by `/<package>/<version>` where `package` is the
        package name (including @scope segment if any) and `version` is the specific version of
        the package that comes from the pnpm-lock.yaml file.

        For example,

        ```
        //:.aspect_rules_js/node_modules/cliui/7.0.4
        ```

        The version may include peer dep(s),

        ```
        //:.aspect_rules_js/node_modules/debug/4.3.4_supports-color@8.1.1
        ```

        It could be also be a url based version,

        ```
        //:.aspect_rules_js/node_modules/debug/github.com/ngokevin/debug/9742c5f383a6f8046241920156236ade8ec30d53
        ```

        Package store link targets names for 3rd party package that come directly from an
        `npm_import` start with `.aspect_rules_js/` then the name passed to the `npm_import`'s `npm_link_imported_package`
        macro (typically `node_modules`) followed by `/<package>/<version>` where `package`
        matches the `package` attribute in the npm_import of the package and `version` matches the
        `version` attribute.

        For example,

        ```
        //:.aspect_rules_js/node_modules/cliui/7.0.4
        ```

        Package store link targets names for 1st party packages automatically linked by `npm_link_all_packages`
        using workspaces will follow the same pattern as 3rd party packages with the version typically defaulting
        to "0.0.0".

        For example,

        ```
        //:.aspect_rules_js/node_modules/@mycorp/mypkg/0.0.0
        ```

        Package store link targets names for 1st party packages manually linked with `npm_link_package`
        start with `.aspect_rules_js/` followed by the name passed to the `npm_link_package`.

        For example,

        ```
        //:.aspect_rules_js/node_modules/@mycorp/mypkg
        ```

        > In typical usage, a node.js program sometimes requires modules which were
        > never declared as dependencies.
        > This pattern is typically used when the program has conditional behavior
        > that is enabled when the module is found (like a plugin) but the program
        > also runs without the dependency.
        > 
        > This is possible because node.js doesn't enforce the dependencies are sound.
        > All files under `node_modules` are available to any program.
        > In contrast, Bazel makes it possible to make builds hermetic, which means that
        > all dependencies of a program must be declared when running in Bazel's sandbox.
        """,
        providers = [NpmPackageStoreInfo],
    ),
    "package": attr.string(
        doc = """The package name to link to.

If unset, the package name in the NpmPackageInfo src must be set.
If set, takes precendance over the package name in the NpmPackageInfo src.
""",
    ),
    "version": attr.string(
        doc = """The package version being linked.

If unset, the package version in the NpmPackageInfo src must be set.
If set, takes precendance over the package version in the NpmPackageInfo src.
""",
    ),
    "dev": attr.bool(
        doc = """Whether this npm package is a dev dependency""",
    ),
    "use_declare_symlink": attr.bool(
        mandatory = True,
        doc = """Whether to use ctx.actions.declare_symlink to create symlinks.

        These are enabled with the `--allow_unresolved_symlinks` flag
        (named `--experimental_allow_unresolved_symlinks in Bazel versions prior to 7.0).

        Typical usage of this rule is via a macro which automatically sets this
        attribute with `select` based on a `config_setting` rule.
        See /js/private/BUILD.bazel in rules_js for an example.
        """,
    ),
    "hardlink": attr.string(
        values = ["auto", "off", "on"],
        default = "auto",
        doc = """Controls when to use hardlinks to files instead of making copies.

        Creating hardlinks is much faster than making copies of files with the caveat that
        hardlinks share file permissions with their source.

        Since Bazel removes write permissions on files in the output tree after an action completes,
        hardlinks to source files is not recommended since write permissions will be inadvertently
        removed from sources files.

        - "auto": hardlinks are used for generated files already in the output tree
        - "off": all files are copied
        - "on": hardlinks are used for all files (not recommended)

        NB: Hardlinking source files in external repositories as was done under the hood
        prior to https://github.com/aspect-build/rules_js/pull/1533 may lead to flaky build
        failures as reported in https://github.com/aspect-build/rules_js/issues/1412.
        """,
    ),
    "verbose": attr.bool(
        doc = """If true, prints out verbose logs to stdout""",
    ),
}

def _npm_package_store_impl(ctx):
    package = ctx.attr.package if ctx.attr.package else ctx.attr.src[NpmPackageInfo].package
    version = ctx.attr.version if ctx.attr.version else ctx.attr.src[NpmPackageInfo].version

    if not package:
        fail("No package name specified to link to. Package name must either be specified explicitly via 'package' attribute or come from the 'src' 'NpmPackageInfo', typically a 'npm_package' target")
    if not version:
        fail("No package version specified to link to. Package version must either be specified explicitly via 'version' attribute or come from the 'src' 'NpmPackageInfo', typically a 'npm_package' target")

    virtual_store_name = utils.virtual_store_name(package, version)

    src_directory = None
    virtual_store_directory = None
    transitive_files = []
    direct_ref_deps = {}

    npm_package_store_deps = []

    if ctx.attr.src:
        # output the package as a TreeArtifact to its virtual store location
        # "node_modules/{virtual_store_root}/{virtual_store_name}/node_modules/{package}"
        virtual_store_directory_path = paths.join("node_modules", utils.virtual_store_root, virtual_store_name, "node_modules", package)

        if ctx.label.workspace_name:
            expected_short_path = paths.join("..", ctx.label.workspace_name, ctx.label.package, virtual_store_directory_path)
        else:
            expected_short_path = paths.join(ctx.label.package, virtual_store_directory_path)
        src_directory = ctx.attr.src[NpmPackageInfo].directory
        if src_directory.short_path == expected_short_path:
            # the input is already the desired output; this is the pattern for
            # packages with lifecycle hooks
            virtual_store_directory = src_directory
        else:
            virtual_store_directory = ctx.actions.declare_directory(virtual_store_directory_path)
            if utils.is_tarball_extension(src_directory.extension):
                # npm packages are always published with one top-level directory inside the tarball, tho the name is not predictable
                # we can use the --strip-components 1 argument with tar to strip one directory level
                args = ctx.actions.args()
                args.add("--extract")
                args.add("--no-same-owner")
                args.add("--no-same-permissions")
                args.add("--strip-components")
                args.add(str(1))
                args.add("--file")
                args.add(src_directory.path)
                args.add("--directory")
                args.add(virtual_store_directory.path)

                bsdtar = ctx.toolchains["@aspect_bazel_lib//lib:tar_toolchain_type"]
                ctx.actions.run(
                    executable = bsdtar.tarinfo.binary,
                    inputs = depset(direct = [src_directory], transitive = [bsdtar.default.files]),
                    outputs = [virtual_store_directory],
                    arguments = [args],
                    mnemonic = "NpmPackageExtract",
                    progress_message = "Extracting npm package {}@{}".format(package, version),
                )
            else:
                copy_directory_bin_action(
                    ctx,
                    src = src_directory,
                    dst = virtual_store_directory,
                    copy_directory_bin = ctx.toolchains["@aspect_bazel_lib//lib:copy_directory_toolchain_type"].copy_directory_info.bin,
                    # Hardlinking source files in external repositories as was done under the hood
                    # prior to https://github.com/aspect-build/rules_js/pull/1533 may lead to flaky build
                    # failures as reported in https://github.com/aspect-build/rules_js/issues/1412.
                    hardlink = ctx.attr.hardlink,
                    verbose = ctx.attr.verbose,
                )

        linked_virtual_store_directories = []
        for dep, _dep_aliases in ctx.attr.deps.items():
            # symlink the package's direct deps to its virtual store location
            if dep[NpmPackageStoreInfo].root_package != ctx.label.package:
                msg = """npm_package_store in %s package cannot depend on npm_package_store in %s package.
deps of npm_package_store must be in the same package.""" % (ctx.label.package, dep[NpmPackageStoreInfo].root_package)
                fail(msg)
            dep_package = dep[NpmPackageStoreInfo].package
            dep_aliases = _dep_aliases.split(",") if _dep_aliases else [dep_package]
            dep_virtual_store_directory = dep[NpmPackageStoreInfo].virtual_store_directory
            if dep_virtual_store_directory:
                linked_virtual_store_directories.append(dep_virtual_store_directory)
                for dep_alias in dep_aliases:
                    # "node_modules/{virtual_store_root}/{virtual_store_name}/node_modules/{package}"
                    dep_symlink_path = paths.join("node_modules", utils.virtual_store_root, virtual_store_name, "node_modules", dep_alias)
                    transitive_files.extend(utils.make_symlink(ctx, dep_symlink_path, dep_virtual_store_directory))
            else:
                # this is a ref npm_link_package, a downstream terminal npm_link_package
                # for this npm dependency will create the dep symlinks for this dep;
                # this pattern is used to break circular dependencies between 3rd
                # party npm deps; it is not recommended for 1st party deps
                direct_ref_deps[dep] = dep_aliases

        for store in ctx.attr.src[NpmPackageInfo].npm_package_store_deps.to_list():
            dep_package = store.package
            dep_virtual_store_directory = store.virtual_store_directory

            # only link npm package store deps from NpmPackageInfo if they have _not_ already been linked directly
            # from deps; fixes https://github.com/aspect-build/rules_js/issues/1110.
            if dep_virtual_store_directory not in linked_virtual_store_directories:
                # "node_modules/{virtual_store_root}/{virtual_store_name}/node_modules/{package}"
                dep_symlink_path = paths.join("node_modules", utils.virtual_store_root, virtual_store_name, "node_modules", dep_package)
                transitive_files.extend(utils.make_symlink(ctx, dep_symlink_path, dep_virtual_store_directory))
                npm_package_store_deps.append(store)
    else:
        # if ctx.attr.src is _not_ set and ctx.attr.deps is, this is a terminal
        # package with deps being the transitive closure of deps;
        # this pattern is used to break circular dependencies between 3rd
        # party npm deps; it is not recommended for 1st party deps
        deps_map = {}
        for dep, _dep_aliases in ctx.attr.deps.items():
            dep_package = dep[NpmPackageStoreInfo].package
            dep_aliases = _dep_aliases.split(",") if _dep_aliases else [dep_package]

            # create a map of deps that have virtual store directories
            if dep[NpmPackageStoreInfo].virtual_store_directory:
                deps_map[utils.virtual_store_name(dep[NpmPackageStoreInfo].package, dep[NpmPackageStoreInfo].version)] = dep
            else:
                # this is a ref npm_link_package, a downstream terminal npm_link_package for this npm
                # depedency will create the dep symlinks for this dep; this pattern is used to break
                # for lifecycle hooks on 3rd party deps; it is not recommended for 1st party deps
                direct_ref_deps[dep] = dep_aliases
        for dep in ctx.attr.deps:
            dep_virtual_store_name = utils.virtual_store_name(dep[NpmPackageStoreInfo].package, dep[NpmPackageStoreInfo].version)
            dep_ref_deps = dep[NpmPackageStoreInfo].ref_deps
            if virtual_store_name == dep_virtual_store_name:
                # provide the node_modules directory for this package if found in the transitive_closure
                virtual_store_directory = dep[NpmPackageStoreInfo].virtual_store_directory
                if virtual_store_directory:
                    transitive_files.append(virtual_store_directory)
            for dep_ref_dep, dep_ref_dep_aliases in dep_ref_deps.items():
                dep_ref_dep_virtual_store_name = utils.virtual_store_name(dep_ref_dep[NpmPackageStoreInfo].package, dep_ref_dep[NpmPackageStoreInfo].version)
                if not dep_ref_dep_virtual_store_name in deps_map:
                    # This can happen in lifecycle npm package targets. We have no choice but to
                    # ignore reference back to self in dyadic circular deps in this case since a
                    # transitive dep on this npm package is impossible in an action that is
                    # outputting the virtual store tree artifact that circular dep would point to.
                    continue
                actual_dep = deps_map[dep_ref_dep_virtual_store_name]
                dep_ref_def_virtual_store_directory = actual_dep[NpmPackageStoreInfo].virtual_store_directory
                if dep_ref_def_virtual_store_directory:
                    for dep_ref_dep_alias in dep_ref_dep_aliases:
                        # "node_modules/{virtual_store_root}/{virtual_store_name}/node_modules/{package}"
                        dep_ref_dep_symlink_path = paths.join("node_modules", utils.virtual_store_root, dep_virtual_store_name, "node_modules", dep_ref_dep_alias)
                        transitive_files.extend(utils.make_symlink(ctx, dep_ref_dep_symlink_path, dep_ref_def_virtual_store_directory))

    files = [virtual_store_directory] if virtual_store_directory else []

    npm_package_store_deps.extend([
        target[NpmPackageStoreInfo]
        for target in ctx.attr.deps
    ])

    files_depset = depset(files)

    transitive_files_depset = depset(files, transitive = [depset(transitive_files)] + [
        npm_package_store.transitive_files
        for npm_package_store in npm_package_store_deps
    ])

    providers = [
        DefaultInfo(
            files = files_depset,
        ),
        NpmPackageStoreInfo(
            root_package = ctx.label.package,
            package = package,
            version = version,
            ref_deps = direct_ref_deps,
            src_directory = src_directory,
            virtual_store_directory = virtual_store_directory,
            files = files_depset,
            transitive_files = transitive_files_depset,
            dev = ctx.attr.dev,
        ),
    ]
    if virtual_store_directory:
        # Provide an output group that provides a single file which is the
        # package directory for use in $(execpath) and $(rootpath).
        # Output group name must match utils.package_directory_output_group
        providers.append(OutputGroupInfo(package_directory = depset([virtual_store_directory])))

    return providers

npm_package_store_lib = struct(
    attrs = _ATTRS,
    implementation = _npm_package_store_impl,
    provides = [DefaultInfo, NpmPackageStoreInfo],
    toolchains = [
        Label("@aspect_bazel_lib//lib:copy_directory_toolchain_type"),
        Label("@aspect_bazel_lib//lib:tar_toolchain_type"),
    ],
)

npm_package_store = rule(
    doc = _DOC,
    implementation = npm_package_store_lib.implementation,
    attrs = npm_package_store_lib.attrs,
    provides = npm_package_store_lib.provides,
    toolchains = npm_package_store_lib.toolchains,
)
