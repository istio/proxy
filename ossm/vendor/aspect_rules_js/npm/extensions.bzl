"""Adapt npm repository rules to be called from MODULE.bazel
See https://bazel.build/docs/bzlmod#extension-definition
"""

load("@bazel_features//:features.bzl", "bazel_features")
load("@aspect_bazel_lib//lib:repo_utils.bzl", "repo_utils")
load("//npm:repositories.bzl", "npm_import", "pnpm_repository", _LATEST_PNPM_VERSION = "LATEST_PNPM_VERSION")
load("//npm/private:npm_translate_lock.bzl", "npm_translate_lock", "npm_translate_lock_lib")
load("//npm/private:npm_translate_lock_helpers.bzl", npm_translate_lock_helpers = "helpers")
load("//npm/private:npm_translate_lock_macro_helpers.bzl", macro_helpers = "helpers")
load("//npm/private:npm_import.bzl", "npm_import_lib", "npm_import_links_lib")
load("//npm/private:npmrc.bzl", "parse_npmrc")
load("//npm/private:transitive_closure.bzl", "translate_to_transitive_closure")
load("//npm/private:utils.bzl", "utils")

LATEST_PNPM_VERSION = _LATEST_PNPM_VERSION

def _npm_extension_impl(module_ctx):
    for mod in module_ctx.modules:
        for attr in mod.tags.npm_translate_lock:
            # TODO(2.0): remove pnpm_version from bzlmod API
            if attr.pnpm_version:
                fail("npm_translate_lock paramater 'pnpm_version' is not supported with bzlmod, use `use_pnpm` instead, received value: '{}'")

            _npm_translate_lock_bzlmod(attr)

            # We cannot read the pnpm_lock file before it has been bootstrapped.
            # See comment in e2e/update_pnpm_lock_with_import/test.sh.
            if attr.pnpm_lock:
                _npm_lock_imports_bzlmod(module_ctx, attr)

        for i in mod.tags.npm_import:
            _npm_import_bzlmod(i)

    if bazel_features.external_deps.extension_metadata_has_reproducible:
        return module_ctx.extension_metadata(
            reproducible = True,
        )
    return module_ctx.extension_metadata()

def _npm_translate_lock_bzlmod(attr):
    npm_translate_lock(
        name = attr.name,
        bins = attr.bins,
        custom_postinstalls = attr.custom_postinstalls,
        data = attr.data,
        dev = attr.dev,
        external_repository_action_cache = attr.external_repository_action_cache,
        generate_bzl_library_targets = attr.generate_bzl_library_targets,
        lifecycle_hooks = attr.lifecycle_hooks,
        lifecycle_hooks_envs = attr.lifecycle_hooks_envs,
        lifecycle_hooks_execution_requirements = attr.lifecycle_hooks_execution_requirements,
        lifecycle_hooks_exclude = attr.lifecycle_hooks_exclude,
        lifecycle_hooks_no_sandbox = attr.lifecycle_hooks_no_sandbox,
        lifecycle_hooks_use_default_shell_env = attr.lifecycle_hooks_use_default_shell_env,
        link_workspace = attr.link_workspace,
        no_optional = attr.no_optional,
        npmrc = attr.npmrc,
        npm_package_lock = attr.npm_package_lock,
        npm_package_target_name = attr.npm_package_target_name,
        package_visibility = attr.package_visibility,
        patches = attr.patches,
        patch_args = attr.patch_args,
        pnpm_lock = attr.pnpm_lock,
        use_pnpm = attr.use_pnpm,
        preupdate = attr.preupdate,
        prod = attr.prod,
        public_hoist_packages = attr.public_hoist_packages,
        quiet = attr.quiet,
        register_copy_directory_toolchains = False,  # this registration is handled elsewhere with bzlmod
        register_copy_to_directory_toolchains = False,  # this registration is handled elsewhere with bzlmod
        register_yq_toolchains = False,  # this registration is handled elsewhere with bzlmod
        register_tar_toolchains = False,  # this registration is handled elsewhere with bzlmod
        replace_packages = attr.replace_packages,
        root_package = attr.root_package,
        run_lifecycle_hooks = attr.run_lifecycle_hooks,
        update_pnpm_lock = attr.update_pnpm_lock,
        use_home_npmrc = attr.use_home_npmrc,
        verify_node_modules_ignored = attr.verify_node_modules_ignored,
        verify_patches = attr.verify_patches,
        yarn_lock = attr.yarn_lock,
        bzlmod = True,
        pnpm_version = None,
    )

def _npm_lock_imports_bzlmod(module_ctx, attr):
    lock_importers = {}
    lock_packages = {}
    lock_patched_dependencies = {}
    lock_parse_err = None
    if attr.use_starlark_yaml_parser:
        lock_importers, lock_packages, lock_patched_dependencies, lock_parse_err = utils.parse_pnpm_lock_yaml(module_ctx.read(attr.pnpm_lock))
    else:
        is_windows = repo_utils.is_windows(module_ctx)
        host_yq = Label("@{}_{}//:yq{}".format(attr.yq_toolchain_prefix, repo_utils.platform(module_ctx), ".exe" if is_windows else ""))
        yq_args = [
            str(module_ctx.path(host_yq)),
            str(module_ctx.path(attr.pnpm_lock)),
            "-o=json",
        ]
        result = module_ctx.execute(yq_args)
        if result.return_code:
            lock_parse_err = "failed to parse pnpm lock file with yq. '{}' exited with {}: \nSTDOUT:\n{}\nSTDERR:\n{}".format(" ".join(yq_args), result.return_code, result.stdout, result.stderr)
        else:
            lock_importers, lock_packages, lock_patched_dependencies, lock_parse_err = utils.parse_pnpm_lock_json(result.stdout if result.stdout != "null" else None)  # NB: yq will return the string "null" if the yaml file is empty

    if lock_parse_err != None:
        msg = """
{type}: pnpm-lock.yaml parse error: {error}`.
""".format(type = "WARNING" if attr.update_pnpm_lock else "ERROR", error = lock_parse_err)

        # buildifier: disable=print
        if attr.update_pnpm_lock:
            print(msg)
        else:
            fail(msg)

    importers, packages = translate_to_transitive_closure(lock_importers, lock_packages, attr.prod, attr.dev, attr.no_optional)
    registries = {}
    npm_auth = {}
    if attr.npmrc:
        npmrc = parse_npmrc(module_ctx.read(attr.npmrc))
        (registries, npm_auth) = npm_translate_lock_helpers.get_npm_auth(npmrc, module_ctx.path(attr.npmrc), module_ctx.os.environ)

    if attr.use_home_npmrc:
        home_directory = utils.home_directory(module_ctx)
        if home_directory:
            home_npmrc_path = "{}/{}".format(home_directory, ".npmrc")
            home_npmrc = parse_npmrc(module_ctx.read(home_npmrc_path))

            (registries2, npm_auth2) = npm_translate_lock_helpers.get_npm_auth(home_npmrc, home_npmrc_path, module_ctx.os.environ)
            registries.update(registries2)
            npm_auth.update(npm_auth2)
        else:
            # buildifier: disable=print
            print("""
WARNING: Cannot determine home directory in order to load home `.npmrc` file in module extension `npm_translate_lock(name = "{attr_name}")`.
""".format(attr_name = attr.name))

    lifecycle_hooks, lifecycle_hooks_execution_requirements, lifecycle_hooks_use_default_shell_env = macro_helpers.macro_lifecycle_args_to_rule_attrs(
        lifecycle_hooks = attr.lifecycle_hooks,
        lifecycle_hooks_exclude = attr.lifecycle_hooks_exclude,
        run_lifecycle_hooks = attr.run_lifecycle_hooks,
        lifecycle_hooks_no_sandbox = attr.lifecycle_hooks_no_sandbox,
        lifecycle_hooks_execution_requirements = attr.lifecycle_hooks_execution_requirements,
        lifecycle_hooks_use_default_shell_env = attr.lifecycle_hooks_use_default_shell_env,
    )
    imports = npm_translate_lock_helpers.get_npm_imports(
        importers = importers,
        packages = packages,
        patched_dependencies = lock_patched_dependencies,
        root_package = attr.pnpm_lock.package,
        rctx_name = attr.name,
        attr = attr,
        all_lifecycle_hooks = lifecycle_hooks,
        all_lifecycle_hooks_execution_requirements = lifecycle_hooks_execution_requirements,
        all_lifecycle_hooks_use_default_shell_env = lifecycle_hooks_use_default_shell_env,
        registries = registries,
        default_registry = utils.default_registry(),
        npm_auth = npm_auth,
    )

    for i in imports:
        npm_import(
            name = i.name,
            bins = i.bins,
            commit = i.commit,
            custom_postinstall = i.custom_postinstall,
            deps = i.deps,
            dev = i.dev,
            integrity = i.integrity,
            generate_bzl_library_targets = attr.generate_bzl_library_targets,
            lifecycle_hooks = i.run_lifecycle_hooks if i.run_lifecycle_hooks and i.lifecycle_hooks else [],
            lifecycle_hooks_env = i.lifecycle_hooks_env if i.run_lifecycle_hooks and i.lifecycle_hooks_env else {},
            lifecycle_hooks_execution_requirements = i.lifecycle_hooks_execution_requirements if i.run_lifecycle_hooks else [],
            lifecycle_hooks_use_default_shell_env = i.lifecycle_hooks_use_default_shell_env,
            link_packages = i.link_packages,
            link_workspace = attr.link_workspace if attr.link_workspace else attr.pnpm_lock.workspace_name,
            npm_auth = i.npm_auth,
            npm_auth_basic = i.npm_auth_basic,
            npm_auth_password = i.npm_auth_password,
            npm_auth_username = i.npm_auth_username,
            package = i.package,
            package_visibility = i.package_visibility,
            patch_args = i.patch_args,
            patches = i.patches,
            replace_package = i.replace_package,
            root_package = i.root_package,
            transitive_closure = i.transitive_closure,
            url = i.url,
            version = i.version,
            register_copy_directory_toolchains = False,  # this registration is handled elsewhere with bzlmod
            register_copy_to_directory_toolchains = False,  # this registration is handled elsewhere with bzlmod
        )

def _npm_import_bzlmod(i):
    npm_import(
        name = i.name,
        bins = i.bins,
        commit = i.commit,
        custom_postinstall = i.custom_postinstall,
        deps = i.deps,
        dev = i.dev,
        extra_build_content = i.extra_build_content,
        integrity = i.integrity,
        lifecycle_hooks = i.lifecycle_hooks,
        lifecycle_hooks_env = i.lifecycle_hooks_env,
        lifecycle_hooks_execution_requirements = i.lifecycle_hooks_execution_requirements,
        lifecycle_hooks_no_sandbox = i.lifecycle_hooks_no_sandbox,
        lifecycle_hooks_use_default_shell_env = i.lifecycle_hooks_use_default_shell_env,
        link_packages = i.link_packages,
        link_workspace = i.link_workspace,
        npm_auth = i.npm_auth,
        npm_auth_basic = i.npm_auth_basic,
        npm_auth_username = i.npm_auth_username,
        npm_auth_password = i.npm_auth_password,
        package = i.package,
        package_visibility = i.package_visibility,
        patch_args = i.patch_args,
        patches = i.patches,
        replace_package = i.replace_package,
        root_package = i.root_package,
        run_lifecycle_hooks = i.run_lifecycle_hooks,
        transitive_closure = i.transitive_closure,
        url = i.url,
        version = i.version,
        register_copy_directory_toolchains = False,  # this registration is handled elsewhere with bzlmod
        register_copy_to_directory_toolchains = False,  # this registration is handled elsewhere with bzlmod
    )

def _npm_translate_lock_attrs():
    attrs = dict(**npm_translate_lock_lib.attrs)

    # Add macro attrs that aren't in the rule attrs.
    attrs["name"] = attr.string()
    attrs["lifecycle_hooks_exclude"] = attr.string_list(default = [])
    attrs["lifecycle_hooks_no_sandbox"] = attr.bool(default = True)

    # Note, pnpm_version can't be a tuple here, so we can't accept the integrity hash.
    # This is okay since you can just call the pnpm module extension below first.
    # TODO(2.0): drop pnpm_version from this module extension
    attrs["pnpm_version"] = attr.string(mandatory = False)
    attrs["run_lifecycle_hooks"] = attr.bool(default = True)

    # Args defaulted differently by the macro
    attrs["npm_package_target_name"] = attr.string(default = "{dirname}")
    attrs["patch_args"] = attr.string_list_dict(default = {"*": ["-p0"]})

    return attrs

def _npm_import_attrs():
    attrs = dict(**npm_import_lib.attrs)
    attrs.update(**npm_import_links_lib.attrs)

    # Add macro attrs that aren't in the rule attrs.
    attrs["name"] = attr.string()
    attrs["lifecycle_hooks_no_sandbox"] = attr.bool(default = False)
    attrs["run_lifecycle_hooks"] = attr.bool(default = False)

    # Args defaulted differently by the macro
    attrs["lifecycle_hooks_execution_requirements"] = attr.string_list(default = ["no-sandbox"])
    attrs["patch_args"] = attr.string_list(default = ["-p0"])
    attrs["package_visibility"] = attr.string_list(default = ["//visibility:public"])

    return attrs

npm = module_extension(
    implementation = _npm_extension_impl,
    tag_classes = {
        "npm_translate_lock": tag_class(attrs = _npm_translate_lock_attrs()),
        "npm_import": tag_class(attrs = _npm_import_attrs()),
    },
)

def _pnpm_extension_impl(module_ctx):
    for mod in module_ctx.modules:
        for attr in mod.tags.pnpm:
            pnpm_repository(
                name = attr.name,
                pnpm_version = (
                    (attr.pnpm_version, attr.pnpm_version_integrity) if attr.pnpm_version_integrity else attr.pnpm_version
                ),
            )

pnpm = module_extension(
    implementation = _pnpm_extension_impl,
    tag_classes = {
        "pnpm": tag_class(
            attrs = {
                "name": attr.string(),
                "pnpm_version": attr.string(default = LATEST_PNPM_VERSION),
                "pnpm_version_integrity": attr.string(),
            },
        ),
    },
)
