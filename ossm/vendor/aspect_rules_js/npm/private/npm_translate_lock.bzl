"""Repository rule to fetch npm packages for a lockfile.

Load this with,

```starlark
load("@aspect_rules_js//npm:repositories.bzl", "npm_translate_lock")
```

These use Bazel's downloader to fetch the packages.
You can use this to redirect all fetches through a store like Artifactory.

See <https://blog.aspect.dev/configuring-bazels-downloader> for more info about how it works
and how to configure it.

[`npm_translate_lock`](#npm_translate_lock) is the primary user-facing API.
It uses the lockfile format from [pnpm](https://pnpm.io/motivation) because it gives us reliable
semantics for how to dynamically lay out `node_modules` trees on disk in bazel-out.

To create `pnpm-lock.yaml`, consider using [`pnpm import`](https://pnpm.io/cli/import)
to preserve the versions pinned by your existing `package-lock.json` or `yarn.lock` file.

If you don't have an existing lock file, you can run `npx pnpm install --lockfile-only`.

Advanced users may want to directly fetch a package from npm rather than start from a lockfile,
[`npm_import`](./npm_import) does this.
"""

load("@bazel_skylib//lib:paths.bzl", "paths")
load("@aspect_bazel_lib//lib:repositories.bzl", _register_copy_directory_toolchains = "register_copy_directory_toolchains", _register_copy_to_directory_toolchains = "register_copy_to_directory_toolchains", _register_tar_toolchains = "register_tar_toolchains", _register_yq_toolchains = "register_yq_toolchains")
load("@aspect_bazel_lib//lib:write_source_files.bzl", "write_source_file")
load(":list_sources.bzl", "list_sources")
load(":npm_translate_lock_generate.bzl", "generate_repository_files")
load(":npm_translate_lock_helpers.bzl", "helpers")
load(":npm_translate_lock_macro_helpers.bzl", macro_helpers = "helpers")
load(":npm_translate_lock_state.bzl", "DEFAULT_ROOT_PACKAGE", "npm_translate_lock_state")
load(":pnpm_repository.bzl", "LATEST_PNPM_VERSION", _pnpm_repository = "pnpm_repository")
load(":utils.bzl", "utils")
load(":transitive_closure.bzl", "translate_to_transitive_closure")

RULES_JS_FROZEN_PNPM_LOCK_ENV = "ASPECT_RULES_JS_FROZEN_PNPM_LOCK"

################################################################################
DEFAULT_REPOSITORIES_BZL_FILENAME = "repositories.bzl"
DEFAULT_DEFS_BZL_FILENAME = "defs.bzl"

_ATTRS = {
    "additional_file_contents": attr.string_list_dict(),
    "bins": attr.string_list_dict(),
    "bzlmod": attr.bool(),
    "custom_postinstalls": attr.string_dict(),
    "data": attr.label_list(),
    "defs_bzl_filename": attr.string(default = DEFAULT_DEFS_BZL_FILENAME),
    "dev": attr.bool(),
    "external_repository_action_cache": attr.string(default = utils.default_external_repository_action_cache()),
    "generate_bzl_library_targets": attr.bool(),
    "replace_packages": attr.string_dict(),
    "lifecycle_hooks_envs": attr.string_list_dict(),
    "lifecycle_hooks_execution_requirements": attr.string_list_dict(),
    "lifecycle_hooks_use_default_shell_env": attr.string_dict(),
    "lifecycle_hooks": attr.string_list_dict(),
    "link_workspace": attr.string(),
    "no_optional": attr.bool(),
    "node_toolchain_prefix": attr.string(default = "nodejs"),
    "npm_package_lock": attr.label(),
    "npm_package_target_name": attr.string(),
    "npmrc": attr.label(),
    "package_visibility": attr.string_list_dict(),
    "patch_args": attr.string_list_dict(),
    "patches": attr.string_list_dict(),
    "use_pnpm": attr.label(default = "@pnpm//:package/bin/pnpm.cjs"),  # bzlmod pnpm extension
    "pnpm_lock": attr.label(),
    "preupdate": attr.label_list(),
    "prod": attr.bool(),
    "public_hoist_packages": attr.string_list_dict(),
    "quiet": attr.bool(default = True),
    "repositories_bzl_filename": attr.string(default = DEFAULT_REPOSITORIES_BZL_FILENAME),
    "root_package": attr.string(default = DEFAULT_ROOT_PACKAGE),
    "update_pnpm_lock": attr.bool(),
    "use_home_npmrc": attr.bool(),
    "use_starlark_yaml_parser": attr.bool(),
    "verify_node_modules_ignored": attr.label(),
    "verify_patches": attr.label(),
    "yarn_lock": attr.label(),
    "yq_toolchain_prefix": attr.string(default = "yq"),
}

npm_translate_lock_lib = struct(
    attrs = _ATTRS,
)

################################################################################
def _npm_translate_lock_impl(rctx):
    rctx.report_progress("Initializing")

    state = npm_translate_lock_state.new(rctx)

    # If a pnpm lock file has not been specified then we need to bootstrap by running `pnpm
    # import` in the user's repository
    if not rctx.attr.pnpm_lock:
        _bootstrap_import(rctx, state)

    if state.should_update_pnpm_lock():
        # Run `pnpm install --lockfile-only` or `pnpm import` if its inputs have changed since last update
        if state.action_cache_miss():
            _fail_if_frozen_pnpm_lock(rctx, state)
            if _update_pnpm_lock(rctx, state):
                if rctx.attr.bzlmod:
                    msg = """

INFO: {} file updated. Please run your build again.

See https://github.com/aspect-build/rules_js/issues/1445
""".format(state.label_store.relative_path("pnpm_lock"))
                    fail(msg)
                else:
                    # If the pnpm lock file was changed then reload it before translation
                    state.reload_lockfile()

    helpers.verify_node_modules_ignored(rctx, state.importers(), state.root_package())

    helpers.verify_patches(rctx, state)

    rctx.report_progress("Translating {}".format(state.label_store.relative_path("pnpm_lock")))

    importers, packages = translate_to_transitive_closure(
        state.importers(),
        state.packages(),
        rctx.attr.prod,
        rctx.attr.dev,
        rctx.attr.no_optional,
    )

    rctx.report_progress("Generating starlark for npm dependencies")

    generate_repository_files(
        rctx,
        state.label_store.label("pnpm_lock"),
        importers,
        packages,
        state.patched_dependencies(),
        state.root_package(),
        state.default_registry(),
        state.npm_registries(),
        state.npm_auth(),
        state.link_workspace(),
    )

npm_translate_lock_rule = repository_rule(
    implementation = _npm_translate_lock_impl,
    attrs = _ATTRS,
)

def npm_translate_lock(
        name,
        pnpm_lock = None,
        npm_package_lock = None,
        yarn_lock = None,
        update_pnpm_lock = None,
        node_toolchain_prefix = "nodejs",
        yq_toolchain_prefix = "yq",
        preupdate = [],
        npmrc = None,
        use_home_npmrc = None,
        data = [],
        patches = {},
        patch_args = {"*": ["-p0"]},
        custom_postinstalls = {},
        package_visibility = {},
        prod = False,
        public_hoist_packages = {},
        dev = False,
        no_optional = False,
        run_lifecycle_hooks = True,
        lifecycle_hooks = {},
        lifecycle_hooks_envs = {},
        lifecycle_hooks_exclude = [],
        lifecycle_hooks_execution_requirements = {},
        lifecycle_hooks_no_sandbox = True,
        lifecycle_hooks_use_default_shell_env = {},
        replace_packages = {},
        bins = {},
        verify_node_modules_ignored = None,
        verify_patches = None,
        quiet = True,
        external_repository_action_cache = utils.default_external_repository_action_cache(),
        link_workspace = None,
        pnpm_version = LATEST_PNPM_VERSION,
        use_pnpm = None,
        register_copy_directory_toolchains = True,
        register_copy_to_directory_toolchains = True,
        register_yq_toolchains = True,
        register_tar_toolchains = True,
        npm_package_target_name = "{dirname}",
        use_starlark_yaml_parser = False,
        # TODO(2.0): remove package_json
        package_json = None,
        # TODO(2.0): remove warn_on_unqualified_tarball_url
        # buildifier: disable=unused-variable
        warn_on_unqualified_tarball_url = None,
        **kwargs):
    """Repository macro to generate starlark code from a lock file.

    In most repositories, it would be an impossible maintenance burden to manually declare all
    of the [`npm_import`](./npm_import) rules. This helper generates an external repository
    containing a helper starlark module `repositories.bzl`, which supplies a loadable macro
    `npm_repositories`. That macro creates an `npm_import` for each package.

    The generated repository also contains:

    - A `defs.bzl` file containing some rules such as `npm_link_all_packages`, which are [documented here](./npm_link_all_packages.md).
    - `BUILD` files declaring targets for the packages listed as `dependencies` or `devDependencies` in `package.json`,
      so you can declare dependencies on those packages without having to repeat version information.

    This macro creates a `pnpm` external repository, if the user didn't create a repository named
    "pnpm" prior to calling `npm_translate_lock`.
    `rules_js` currently only uses this repository when `npm_package_lock` or `yarn_lock` are used.
    Set `pnpm_version` to `None` to inhibit this repository creation.

    For more about how to use npm_translate_lock, read [pnpm and rules_js](/docs/pnpm.md).

    Args:
        name: The repository rule name

        pnpm_lock: The `pnpm-lock.yaml` file.

        npm_package_lock: The `package-lock.json` file written by `npm install`.

            Only one of `npm_package_lock` or `yarn_lock` may be set.

        yarn_lock: The `yarn.lock` file written by `yarn install`.

            Only one of `npm_package_lock` or `yarn_lock` may be set.

        update_pnpm_lock: When True, the pnpm lock file will be updated automatically when any of its inputs
            have changed since the last update.

            Defaults to True when one of `npm_package_lock` or `yarn_lock` are set.
            Otherwise it defaults to False.

            Read more: [using update_pnpm_lock](/docs/pnpm.md#update_pnpm_lock)

        node_toolchain_prefix: the prefix of the node toolchain to use when generating the pnpm lockfile.

        yq_toolchain_prefix: the prefix of the yq toolchain to use for parsing the pnpm lockfile.

        preupdate: Node.js scripts to run in this repository rule before auto-updating the pnpm lock file.

            Scripts are run sequentially in the order they are listed. The working directory is set to the root of the
            external repository. Make sure all files required by preupdate scripts are added to the `data` attribute.

            A preupdate script could, for example, transform `resolutions` in the root `package.json` file from a format
            that yarn understands such as `@foo/**/bar` to the equivalent `@foo/*>bar` that pnpm understands so that
            `resolutions` are compatible with pnpm when running `pnpm import` to update the pnpm lock file.

            Only needed when `update_pnpm_lock` is True.
            Read more: [using update_pnpm_lock](/docs/pnpm.md#update_pnpm_lock)

        npmrc: The `.npmrc` file, if any, to use.

            When set, the `.npmrc` file specified is parsed and npm auth tokens and basic authentication configuration
            specified in the file are passed to the Bazel downloader for authentication with private npm registries.

            In a future release, pnpm settings such as public-hoist-patterns will be used.

        use_home_npmrc: Use the `$HOME/.npmrc` file (or `$USERPROFILE/.npmrc` when on Windows) if it exists.

            Settings from home `.npmrc` are merged with settings loaded from the `.npmrc` file specified
            in the `npmrc` attribute, if any. Where there are conflicting settings, the home `.npmrc` values
            will take precedence.

            WARNING: The repository rule will not be invalidated by changes to the home `.npmrc` file since there
            is no way to specify this file as an input to the repository rule. If changes are made to the home
            `.npmrc` you can force the repository rule to re-run and pick up the changes by running:
            `bazel run @{name}//:sync` where `name` is the name of the `npm_translate_lock` you want to re-run.

            Because of the repository rule invalidation issue, using the home `.npmrc` is not recommended.
            `.npmrc` settings should generally go in the `npmrc` in your repository so they are shared by all
            developers. The home `.npmrc` should be reserved for authentication settings for private npm repositories.

        data: Data files required by this repository rule when auto-updating the pnpm lock file.

            Only needed when `update_pnpm_lock` is True.
            Read more: [using update_pnpm_lock](/docs/pnpm.md#update_pnpm_lock)

        patches: A map of package names or package names with their version (e.g., "my-package" or "my-package@v1.2.3")
            to a label list of patches to apply to the downloaded npm package. Multiple matches are additive.

            These patches are applied after any patches in [pnpm.patchedDependencies](https://pnpm.io/next/package_json#pnpmpatcheddependencies).

            Read more: [patching](/docs/pnpm.md#patching)

        patch_args: A map of package names or package names with their version (e.g., "my-package" or "my-package@v1.2.3")
            to a label list arguments to pass to the patch tool. The most specific match wins.

            Read more: [patching](/docs/pnpm.md#patching)

        custom_postinstalls: A map of package names or package names with their version (e.g., "my-package" or "my-package@v1.2.3")
            to a custom postinstall script to apply to the downloaded npm package after its lifecycle scripts runs.
            If the version is left out of the package name, the script will run on every version of the npm package. If
            a custom postinstall scripts exists for a package as well as for a specific version, the script for the versioned package
            will be appended with `&&` to the non-versioned package script.

            For example,

            ```
            custom_postinstalls = {
                "@foo/bar": "echo something > somewhere.txt",
                "fum@0.0.1": "echo something_else > somewhere_else.txt",
            },
            ```

            Custom postinstalls are additive and joined with ` && ` when there are multiple matches for a package.
            More specific matches are appended to previous matches.

        package_visibility: A map of package names or package names with their version (e.g., "my-package" or "my-package@v1.2.3")
            to a visibility list to use for the package's generated node_modules link targets. Multiple matches are additive.
            If there are no matches then the package's generated node_modules link targets default to public visibility
            (`["//visibility:public"]`).

        prod: If True, only install `dependencies` but not `devDependencies`.

        public_hoist_packages: A map of package names or package names with their version (e.g., "my-package" or "my-package@v1.2.3")
            to a list of Bazel packages in which to hoist the package to the top-level of the node_modules tree when linking.

            This is similar to setting https://pnpm.io/npmrc#public-hoist-pattern in an .npmrc file outside of Bazel, however,
            wild-cards are not yet supported and npm_translate_lock will fail if there are multiple versions of a package that
            are to be hoisted.

            ```
            public_hoist_packages = {
                "@foo/bar": [""] # link to the root package in the WORKSPACE
                "fum@0.0.1": ["some/sub/package"]
            },
            ```

            List of public hoist packages are additive when there are multiple matches for a package. More specific matches
            are appended to previous matches.

        dev: If True, only install `devDependencies`

        no_optional: If True, `optionalDependencies` are not installed.

            Currently `npm_translate_lock` behaves differently from pnpm in that is downloads all `optionaDependencies`
            while pnpm doesn't download `optionalDependencies` that are not needed for the platform pnpm is run on.
            See https://github.com/pnpm/pnpm/pull/3672 for more context.

        run_lifecycle_hooks: Sets a default value for `lifecycle_hooks` if `*` not already set.
            Set this to `False` to disable lifecycle hooks.

        lifecycle_hooks: A dict of package names to list of lifecycle hooks to run for that package.

            By default the `preinstall`, `install` and `postinstall` hooks are run if they exist. This attribute allows
            the default to be overridden for packages to run `prepare`.

            List of hooks are not additive. The most specific match wins.

            Read more: [lifecycles](/docs/pnpm.md#lifecycles)

        lifecycle_hooks_exclude: A list of package names or package names with their version (e.g., "my-package" or "my-package@v1.2.3")
            to not run any lifecycle hooks on.

            Equivalent to adding `<value>: []` to `lifecycle_hooks`.

            Read more: [lifecycles](/docs/pnpm.md#lifecycles)

        lifecycle_hooks_envs: Environment variables set for the lifecycle hooks actions on npm packages.
            The environment variables can be defined per package by package name or globally using "*".
            Variables are declared as key/value pairs of the form "key=value".
            Multiple matches are additive.

            Read more: [lifecycles](/docs/pnpm.md#lifecycles)

        lifecycle_hooks_execution_requirements: Execution requirements applied to the preinstall, install and postinstall
            lifecycle hooks on npm packages.

            The execution requirements can be defined per package by package name or globally using "*".

            Execution requirements are not additive. The most specific match wins.

            Read more: [lifecycles](/docs/pnpm.md#lifecycles)

        lifecycle_hooks_no_sandbox: If True, a "no-sandbox" execution requirement is added to all lifecycle hooks
            unless overridden by `lifecycle_hooks_execution_requirements`.

            Equivalent to adding `"*": ["no-sandbox"]` to `lifecycle_hooks_execution_requirements`.

            This defaults to True to limit the overhead of sandbox creation and copying the output
            TreeArtifacts out of the sandbox.

            Read more: [lifecycles](/docs/pnpm.md#lifecycles)

        lifecycle_hooks_use_default_shell_env: The `use_default_shell_env` attribute of the lifecycle hooks
            actions on npm packages.

            See [use_default_shell_env](https://bazel.build/rules/lib/builtins/actions#run.use_default_shell_env)

            Note: [--incompatible_merge_fixed_and_default_shell_env](https://bazel.build/reference/command-line-reference#flag--incompatible_merge_fixed_and_default_shell_env)
            is often required and not enabled by default in Bazel < 7.0.0.

            This defaults to False reduce the negative effects of `use_default_shell_env`. Requires bazel-lib >= 2.4.2.

            Read more: [lifecycles](/docs/pnpm.md#lifecycles)

        replace_packages: A dict of package names to npm_package targets to link instead of the sources specified in the pnpm lock file for the corresponding packages.

            The injected npm_package targets may optionally contribute transitive npm package dependencies on top
            of the transitive dependencies specified in the pnpm lock file for their respective packages, however, these
            transitive dependencies must not collide with pnpm lock specified transitive dependencies.

            Any patches specified for the packages will be not applied to the injected npm_package targets. They
            will be applied, however, to the fetches sources for their respecitve packages so they can still be useful
            for patching the fetched `package.json` files, which are used to determine the generated bin entries for packages.

            NB: lifecycle hooks and custom_postinstall scripts, if implicitly or explicitly enabled, will be run on
            the injected npm_package targets. These may be disabled explicitly using the `lifecycle_hooks` attribute.

        bins: Binary files to create in `node_modules/.bin` for packages in this lock file.

            For a given package, this is typically derived from the "bin" attribute in
            the package.json file of that package.

            For example:

            ```
            bins = {
                "@foo/bar": {
                    "foo": "./foo.js",
                    "bar": "./bar.js"
                },
            }
            ```

            Dicts of bins not additive. The most specific match wins.

            In the future, this field may be automatically populated from information in the pnpm lock
            file. That feature is currently blocked on https://github.com/pnpm/pnpm/issues/5131.

            Note: Bzlmod users must use an alternative syntax due to module extensions not supporting
            dict-of-dict attributes:

            ```
            bins = {
                "@foo/bar": [
                    "foo=./foo.js",
                    "bar=./bar.js"
                ],
            }
            ```

        verify_node_modules_ignored: node_modules folders in the source tree should be ignored by Bazel.

            This points to a `.bazelignore` file to verify that all nested node_modules directories
            pnpm will create are listed.

            See https://github.com/bazelbuild/bazel/issues/8106

        verify_patches: Label to a patch list file.

            Use this in together with the `list_patches` macro to guarantee that all patches in a patch folder
            are included in the `patches` attribute.

            For example:

            ```
            verify_patches = "//patches:patches.list",
            ```

            In your patches folder add a BUILD.bazel file containing.
            ```
            load("@aspect_rules_js//npm:repositories.bzl", "list_patches")

            list_patches(
                name = "patches",
                out = "patches.list",
            )
            ```

            Once you have created this file, you need to create an empty `patches.list` file before generating the first list. You can do this by running
            ```
            touch patches/patches.list
            ```

            Finally, write the patches file at least once to make sure all patches are listed. This can be done by running `bazel run //patches:patches_update`.

            See the `list_patches` documentation for further info.
            NOTE: if you would like to customize the patches directory location, you can set a flag in the `.npmrc`. Here is an example of what this might look like
            ```
            # Set the directory for pnpm when patching
            # https://github.com/pnpm/pnpm/issues/6508#issuecomment-1537242124
            patches-dir=bazel/js/patches
            ```
            If you do this, you will have to update the `verify_patches` path to be this path instead of `//patches` like above.

        quiet: Set to False to print info logs and output stdout & stderr of pnpm lock update actions to the console.

        external_repository_action_cache: The location of the external repository action cache to write to when `update_pnpm_lock` = True.

        link_workspace: The workspace name where links will be created for the packages in this lock file.

            This is typically set in rule sets and libraries that vendor the starlark generated by npm_translate_lock
            so the link_workspace passed to npm_import is set correctly so that links are created in the external
            repository and not the user workspace.

            Can be left unspecified if the link workspace is the user workspace.

        pnpm_version: pnpm version to use when generating the @pnpm repository. Set to None to not create this repository.

            Can be left unspecified and the rules_js default `LATEST_PNPM_VERSION` will be used.

            Use `use_pnpm` for bzlmod.

        use_pnpm: label of the pnpm extension to use.

            Can be left unspecified and the rules_js default pnpm extension (with the `LATEST_PNPM_VERSION`) will be used.

            Use `pnpm_version` for non-bzlmod.

        register_copy_directory_toolchains: if True, `@aspect_bazel_lib//lib:repositories.bzl` `register_copy_directory_toolchains()` is called if the toolchain is not already registered

        register_copy_to_directory_toolchains: if True, `@aspect_bazel_lib//lib:repositories.bzl` `register_copy_to_directory_toolchains()` is called if the toolchain is not already registered

        register_yq_toolchains: if True, `@aspect_bazel_lib//lib:repositories.bzl` `register_yq_toolchains()` is called if the toolchain is not already registered

        register_tar_toolchains: if True, `@aspect_bazel_lib//lib:repositories.bzl` `register_tar_toolchains()` is called if the toolchain is not already registered

        package_json: Deprecated.

            Add all `package.json` files that are part of the workspace to `data` instead.

        warn_on_unqualified_tarball_url: Deprecated. Will be removed in next major release.

        npm_package_target_name: The name of linked `npm_package` targets. When `npm_package` targets are linked as pnpm workspace
            packages, the name of the target must align with this value.

            The `{dirname}` placeholder is replaced with the directory name of the target.

            By default the directory name of the target is used.

            Default: `{dirname}`

        use_starlark_yaml_parser: Opt-out of using `yq` to parse the pnpm-lock file which was added
            in https://github.com/aspect-build/rules_js/pull/1458 and use the legacy starlark yaml
            parser instead.

            This opt-out is a return safety in cases where yq is not able to parse the pnpm generated
            yaml file. For example, this has been observed to happen due to a line such as the following
            in the pnpm generated lock file:

            ```
            resolution: {tarball: https://gitpkg.vercel.app/blockprotocol/blockprotocol/packages/%40blockprotocol/type-system-web?6526c0e}
            ```

            where the `?` character in the `tarball` value causes `yq` to fail with:

            ```
            $ yq pnpm-lock.yaml -o=json
            Error: bad file 'pnpm-lock.yaml': yaml: line 7129: did not find expected ',' or '}'
            ```

            If the tarball value is quoted or escaped then yq would accept it but as of this writing, the latest
            version of pnpm (8.14.3) does not quote or escape such a value and the latest version of yq (4.40.5)
            does not handle it as is.

            Possibly related to https://github.com/pnpm/pnpm/issues/5414.

        **kwargs: Internal use only
    """

    # TODO(2.0): remove backward compat support for update_pnpm_lock_node_toolchain_prefix
    update_pnpm_lock_node_toolchain_prefix = kwargs.pop("update_pnpm_lock_node_toolchain_prefix", None)

    # TODO(2.0): move this to a new required rules_js_repositories() WORKSPACE function
    if register_copy_directory_toolchains and not native.existing_rule("copy_directory_toolchains"):
        _register_copy_directory_toolchains()
    if register_copy_to_directory_toolchains and not native.existing_rule("copy_to_directory_toolchains"):
        _register_copy_to_directory_toolchains()
    if register_yq_toolchains and not native.existing_rule("yq_toolchains"):
        _register_yq_toolchains()
    if register_tar_toolchains and not native.existing_rule("tar_toolchains"):
        _register_tar_toolchains()

    # Gather undocumented attributes
    root_package = kwargs.pop("root_package", None)
    additional_file_contents = kwargs.pop("additional_file_contents", {})
    repositories_bzl_filename = kwargs.pop("repositories_bzl_filename", None)
    defs_bzl_filename = kwargs.pop("defs_bzl_filename", None)
    generate_bzl_library_targets = kwargs.pop("generate_bzl_library_targets", None)
    bzlmod = kwargs.pop("bzlmod", False)

    if len(kwargs):
        msg = "Invalid npm_translate_lock parameter '{}'".format(kwargs.keys()[0])
        fail(msg)

    if not bzlmod and pnpm_version != None:
        _pnpm_repository(name = "pnpm", pnpm_version = pnpm_version)

    if yarn_lock:
        data = data + [yarn_lock]

    if npm_package_lock:
        data = data + [npm_package_lock]

    if package_json:
        data = data + [package_json]

        # buildifier: disable=print
        print("""
WARNING: `package_json` attribute in `npm_translate_lock(name = "{name}")` is deprecated. Add all package.json files to the `data` attribute instead.
""".format(name = name))

    # convert bins to a string_list_dict to satisfy attr type in repository rule
    bins_string_list_dict = {}
    if type(bins) != "dict":
        fail("Expected bins to be a dict")
    for key, value in bins.items():
        if type(value) == "list":
            # The passed 'bins' value is already in the dict-of-string-list
            # form needed by the rule. This is undocumented but necessary for
            # the bzlmod interface to use this macro since dict-of-dicts attributes
            # cannot be passed into module extension attrs.
            bins_string_list_dict = bins
            break
        if type(value) != "dict":
            fail("Expected values in bins to be a dicts")
        if key not in bins_string_list_dict:
            bins_string_list_dict[key] = []
        for value_key, value_value in value.items():
            bins_string_list_dict[key].append("{}={}".format(value_key, value_value))

    # Default update_pnpm_lock to True if npm_package_lock or yarn_lock is set to
    # preserve pre-update_pnpm_lock `pnpm import` behavior.
    if update_pnpm_lock == None and (npm_package_lock or yarn_lock):
        update_pnpm_lock = True

    if not update_pnpm_lock and preupdate:
        fail("expected update_pnpm_lock to be True when preupdate are specified")

    lifecycle_hooks, lifecycle_hooks_execution_requirements, lifecycle_hooks_use_default_shell_env = macro_helpers.macro_lifecycle_args_to_rule_attrs(
        lifecycle_hooks,
        lifecycle_hooks_exclude,
        run_lifecycle_hooks,
        lifecycle_hooks_no_sandbox,
        lifecycle_hooks_execution_requirements,
        lifecycle_hooks_use_default_shell_env,
    )

    npm_translate_lock_rule(
        name = name,
        pnpm_lock = pnpm_lock,
        npm_package_lock = npm_package_lock,
        yarn_lock = yarn_lock,
        update_pnpm_lock = update_pnpm_lock,
        npmrc = npmrc,
        use_home_npmrc = use_home_npmrc,
        patches = patches,
        patch_args = patch_args,
        custom_postinstalls = custom_postinstalls,
        package_visibility = package_visibility,
        prod = prod,
        public_hoist_packages = public_hoist_packages,
        dev = dev,
        no_optional = no_optional,
        lifecycle_hooks = lifecycle_hooks,
        lifecycle_hooks_envs = lifecycle_hooks_envs,
        lifecycle_hooks_execution_requirements = lifecycle_hooks_execution_requirements,
        lifecycle_hooks_use_default_shell_env = lifecycle_hooks_use_default_shell_env,
        replace_packages = replace_packages,
        bins = bins_string_list_dict,
        verify_node_modules_ignored = verify_node_modules_ignored,
        verify_patches = verify_patches,
        external_repository_action_cache = external_repository_action_cache,
        link_workspace = link_workspace,
        root_package = root_package,
        additional_file_contents = additional_file_contents,
        repositories_bzl_filename = repositories_bzl_filename,
        defs_bzl_filename = defs_bzl_filename,
        generate_bzl_library_targets = generate_bzl_library_targets,
        data = data,
        preupdate = preupdate,
        quiet = quiet,
        node_toolchain_prefix = update_pnpm_lock_node_toolchain_prefix if update_pnpm_lock_node_toolchain_prefix else node_toolchain_prefix,
        use_pnpm = use_pnpm,
        yq_toolchain_prefix = yq_toolchain_prefix,
        npm_package_target_name = npm_package_target_name,
        use_starlark_yaml_parser = use_starlark_yaml_parser,
        bzlmod = bzlmod,
    )

def list_patches(name, out = None, include_patterns = ["*.diff", "*.patch"], exclude_patterns = []):
    """Write a file containing a list of all patches in the current folder to the source tree.

    Use this together with the `verify_patches` attribute of `npm_translate_lock` to verify
    that all patches in a patch folder are included. This macro stamps a test to ensure the
    file stays up to date.

    Args:
        name: Name of the target
        out: Name of file to write to the source tree. If unspecified, `name` is used
        include_patterns: Patterns to pass to a glob of patch files
        exclude_patterns: Patterns to ignore in a glob of patch files
    """
    outfile = out if out else name

    # Ignore the patch list file we generate
    exclude_patterns = exclude_patterns[:]
    exclude_patterns.append(outfile)

    list_sources(
        name = "%s_list" % name,
        srcs = native.glob(include_patterns, exclude = exclude_patterns),
    )

    write_source_file(
        name = "%s_update" % name,
        in_file = ":%s_list" % name,
        out_file = outfile,
    )

################################################################################
def _bootstrap_import(rctx, state):
    pnpm_lock_label = state.label_store.label("pnpm_lock")
    pnpm_lock_path = state.label_store.path("pnpm_lock")

    # Check if the pnpm lock file already exists and copy it over if it does.
    # When we do this, warn the user that we do.
    if utils.exists(rctx, pnpm_lock_path):
        # buildifier: disable=print
        print("""
WARNING: Implicitly using pnpm-lock.yaml file `{pnpm_lock}` that is expected to be the result of running `pnpm import` on the `{lock}` lock file.
         Set the `pnpm_lock` attribute of `npm_translate_lock(name = "{rctx_name}")` to `{pnpm_lock}` suppress this warning.
""".format(pnpm_lock = pnpm_lock_label, lock = state.label_store.label("lock"), rctx_name = rctx.name))
        return

    # No pnpm lock file exists and the user has specified a yarn or npm lock file. Bootstrap
    # the pnpm lock file by running `pnpm import` in the source tree. We run in the source tree
    # because at this point the user has likely not added all package.json and data files that
    # pnpm import depends on to `npm_translate_lock`. In order to get a complete initial pnpm lock
    # file with all workspace package imports listed we likely need to run in the source tree.
    bootstrap_working_directory = paths.dirname(pnpm_lock_path)

    if not rctx.attr.quiet:
        # buildifier: disable=print
        print("""
INFO: Running initial `pnpm import` in `{wd}` to bootstrap the pnpm-lock.yaml file required by rules_js.
      It is recommended that you check the generated pnpm-lock.yaml file into source control and add it to the pnpm_lock
      attribute of `npm_translate_lock(name = "{rctx_name}")` so subsequent invocations of the repository
      rule do not need to run `pnpm import` unless an input has changed.""".format(wd = bootstrap_working_directory, rctx_name = rctx.name))

    rctx.report_progress("Bootstrapping pnpm-lock.yaml file with `pnpm import`")

    result = rctx.execute(
        [
            state.label_store.path("host_node"),
            state.label_store.path("pnpm_entry"),
            "import",
        ],
        working_directory = bootstrap_working_directory,
        quiet = rctx.attr.quiet,
    )
    if result.return_code:
        msg = """ERROR: 'pnpm import' exited with status {status}:
STDOUT:
{stdout}
STDERR:
{stderr}
""".format(status = result.return_code, stdout = result.stdout, stderr = result.stderr)
        fail(msg)

    if not utils.exists(rctx, pnpm_lock_path):
        msg = """

ERROR: Running `pnpm import` did not generate the {path} file.
       Try installing pnpm (https://pnpm.io/installation) and running `pnpm import` manually
       to generate the pnpm-lock.yaml file.""".format(path = pnpm_lock_path)
        fail(msg)

    msg = """

INFO: Initial pnpm-lock.yaml file generated. Please add the generated pnpm-lock.yaml file into
      source control and set the `pnpm_lock` attribute in `npm_translate_lock(name = "{rctx_name}")` to `{pnpm_lock}`
      and then run your build again.""".format(
        rctx_name = rctx.name,
        pnpm_lock = pnpm_lock_label,
    )
    fail(msg)

################################################################################
def _execute_preupdate_scripts(rctx, state):
    for i in range(len(rctx.attr.preupdate)):
        script_key = "preupdate_{}".format(i)

        rctx.report_progress("Executing preupdate Node.js script `{script}`".format(
            script = state.label_store.relative_path(script_key),
        ))

        result = rctx.execute(
            [
                state.label_store.path("host_node"),
                state.label_store.path(script_key),
            ],
            # To keep things simple, run at the root of the external repository
            working_directory = state.label_store.repo_root,
            quiet = rctx.attr.quiet,
        )
        if result.return_code:
            msg = """

ERROR: `node {script}` exited with status {status}.

       Make sure all package.json and other data files required for the running `node {script}` are added to
       the data attribute of `npm_translate_lock(name = "{rctx_name}")`.

STDOUT:
{stdout}
STDERR:
{stderr}
""".format(
                script = state.label_store.relative_path(script_key),
                rctx_name = rctx.name,
                status = result.return_code,
                stderr = result.stderr,
                stdout = result.stdout,
            )
            fail(msg)

################################################################################
def _update_pnpm_lock(rctx, state):
    _execute_preupdate_scripts(rctx, state)

    pnpm_lock_label = state.label_store.label("pnpm_lock")
    pnpm_lock_relative_path = state.label_store.relative_path("pnpm_lock")

    update_cmd = ["import"] if rctx.attr.npm_package_lock or rctx.attr.yarn_lock else ["install", "--lockfile-only"]
    update_working_directory = paths.dirname(state.label_store.repository_path("pnpm_lock"))

    pnpm_cmd = " ".join(update_cmd)

    if not rctx.attr.quiet:
        # buildifier: disable=print
        print("""
INFO: Updating `{pnpm_lock}` file as its inputs have changed since the last update.
      Running `pnpm {pnpm_cmd}` in `{wd}`.
      To disable this feature set `update_pnpm_lock` to False in `npm_translate_lock(name = "{rctx_name}")`.""".format(
            pnpm_lock = pnpm_lock_relative_path,
            pnpm_cmd = pnpm_cmd,
            wd = update_working_directory,
            rctx_name = rctx.name,
        ))

    rctx.report_progress("Updating pnpm-lock.yaml with `pnpm {pnpm_cmd}`".format(pnpm_cmd = pnpm_cmd))

    result = rctx.execute(
        [
            state.label_store.path("host_node"),
            state.label_store.path("pnpm_entry"),
        ] + update_cmd,
        # Run pnpm in the external repository so that we are hermetic and all data files that are required need
        # to be specified. This requirement means that if any data file changes then the update command will be
        # re-run. For cases where all data files cannot be specified a user can simply turn off auto-updates
        # by setting update_pnpm_lock to False and update their pnpm-lock.yaml file manually.
        working_directory = update_working_directory,
        quiet = rctx.attr.quiet,
    )
    if result.return_code:
        msg = """

ERROR: `pnpm {cmd}` exited with status {status}.

       Make sure all package.json and other data files required for the running `pnpm {cmd}` are added to
       the data attribute of `npm_translate_lock(name = "{rctx_name}")`.

       If the problem persists, install pnpm (https://pnpm.io/installation) and run `pnpm {cmd}`
       manually to update the pnpm-lock.yaml file. If you have specified `preupdate` scripts in
       `npm_translate_lock(name = "{rctx_name}")` you may have to run these manually as well.

STDOUT:
{stdout}
STDERR:
{stderr}
""".format(
            cmd = " ".join(update_cmd),
            rctx_name = rctx.name,
            status = result.return_code,
            stderr = result.stderr,
            stdout = result.stdout,
        )
        fail(msg)

    lockfile_changed = False
    if state.set_input_hash(
        state.label_store.relative_path("pnpm_lock"),
        utils.hash(rctx.read(state.label_store.repository_path("pnpm_lock"))),
    ):
        # The lock file has changed
        if not rctx.attr.quiet:
            # buildifier: disable=print
            print("""
INFO: {} file has changed""".format(pnpm_lock_relative_path))
        utils.reverse_force_copy(rctx, pnpm_lock_label)
        lockfile_changed = True

    state.write_action_cache()

    return lockfile_changed

################################################################################
def _fail_if_frozen_pnpm_lock(rctx, state):
    if RULES_JS_FROZEN_PNPM_LOCK_ENV in rctx.os.environ.keys() and rctx.os.environ[RULES_JS_FROZEN_PNPM_LOCK_ENV]:
        fail("""

ERROR: `{action_cache}` is out of date. `{pnpm_lock}` may require an update. To update run,

           bazel run @{rctx_name}//:sync

""".format(
            action_cache = state.label_store.relative_path("action_cache"),
            pnpm_lock = state.label_store.relative_path("pnpm_lock"),
            rctx_name = rctx.name,
        ))
