"""
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
"""

load("@aspect_bazel_lib//lib:repo_utils.bzl", "patch", "repo_utils")
load("@aspect_bazel_lib//lib:repositories.bzl", _register_copy_directory_toolchains = "register_copy_directory_toolchains", _register_copy_to_directory_toolchains = "register_copy_to_directory_toolchains")
load("@aspect_bazel_lib//lib:utils.bzl", "is_bazel_6_or_greater")
load("@bazel_skylib//lib:dicts.bzl", "dicts")
load("@bazel_skylib//lib:paths.bzl", "paths")
load(
    "@bazel_tools//tools/build_defs/repo:git_worker.bzl",
    _git_add_origin = "add_origin",
    _git_clean = "clean",
    _git_fetch = "fetch",
    _git_init = "init",
    _git_reset = "reset",
)
load(":starlark_codegen_utils.bzl", "starlark_codegen_utils")
load(":utils.bzl", "utils")

_LINK_JS_PACKAGE_LOADS_TMPL = """\
# buildifier: disable=bzl-visibility
load("@aspect_rules_js//npm/private:npm_package_store_internal.bzl", _npm_package_store = "npm_package_store_internal")

# buildifier: disable=bzl-visibility
load("@aspect_rules_js//npm/private:npm_link_package_store.bzl", _npm_link_package_store = "npm_link_package_store")\
"""

_LINK_JS_PACKAGE_LIFECYCLE_LOADS_TMPL = """\
load("@aspect_rules_js//js:defs.bzl", _js_run_binary = "js_run_binary")

# buildifier: disable=bzl-visibility
load("@aspect_rules_js//npm/private:npm_package_internal.bzl", _npm_package_internal = "npm_package_internal")\
"""

_LINK_JS_PACKAGE_TMPL = """
# Generated npm_package_store targets for npm package {package}@{version}
# buildifier: disable=function-docstring
def npm_imported_package_store(name):
    root_package = "{root_package}"
    is_root = native.package_name() == root_package
    if not is_root:
        msg = "No store links in bazel package '%s' for npm package npm package {package}@{version}. This is neither the root package nor a link package of this package." % native.package_name()
        fail(msg)
    if not name.endswith("/{package}"):
        msg = "name must end with one of '/{package}' when linking the store in package '{package}'; recommended value is 'node_modules/{package}'"
        fail(msg)
    link_root_name = name[:-len("/{package}")]

    deps = {deps}
    ref_deps = {ref_deps}

    store_target_name = "{virtual_store_root}/{{}}/{virtual_store_name}".format(link_root_name)

    # reference target used to avoid circular deps
    _npm_package_store(
        name = "{{}}/ref".format(store_target_name),
        package = "{package}",
        version = "{version}",
        dev = {dev},
        tags = ["manual"],
        use_declare_symlink = select({{
            Label("@aspect_rules_js//js:allow_unresolved_symlinks"): True,
            "//conditions:default": False,
        }}),
    )

    # post-lifecycle target with reference deps for use in terminal target with transitive closure
    _npm_package_store(
        name = "{{}}/pkg".format(store_target_name),
        src = "{{}}/pkg_lc".format(store_target_name) if {has_lifecycle_build_target} else "{npm_package_target}",
        package = "{package}",
        version = "{version}",
        dev = {dev},
        deps = ref_deps,
        tags = ["manual"],
        use_declare_symlink = select({{
            Label("@aspect_rules_js//js:allow_unresolved_symlinks"): True,
            "//conditions:default": False,
        }}),
    )

    # virtual store target with transitive closure of all npm package dependencies
    _npm_package_store(
        name = store_target_name,
        src = None if {transitive_closure_pattern} else "{npm_package_target}",
        package = "{package}",
        version = "{version}",
        dev = {dev},
        deps = deps,
        visibility = ["//visibility:public"],
        tags = ["manual"],
        use_declare_symlink = select({{
            Label("@aspect_rules_js//js:allow_unresolved_symlinks"): True,
            "//conditions:default": False,
        }}),
    )

    # filegroup target that provides a single file which is
    # package directory for use in $(execpath) and $(rootpath)
    native.filegroup(
        name = "{{}}/dir".format(store_target_name),
        srcs = [":{{}}".format(store_target_name)],
        output_group = "{package_directory_output_group}",
        visibility = ["//visibility:public"],
        tags = ["manual"],
    )
"""

_LINK_JS_PACKAGE_LIFECYCLE_TMPL = """\
    lc_deps = {lc_deps}

    # pre-lifecycle target with reference deps for use terminal pre-lifecycle target
    _npm_package_store(
        name = "{{}}/pkg_pre_lc_lite".format(store_target_name),
        package = "{package}",
        version = "{version}",
        dev = {dev},
        deps = ref_deps,
        tags = ["manual"],
        use_declare_symlink = select({{
            Label("@aspect_rules_js//js:allow_unresolved_symlinks"): True,
            "//conditions:default": False,
        }}),
    )

    # terminal pre-lifecycle target for use in lifecycle build target below
    _npm_package_store(
        name = "{{}}/pkg_pre_lc".format(store_target_name),
        package = "{package}",
        version = "{version}",
        dev = {dev},
        deps = lc_deps,
        tags = ["manual"],
        use_declare_symlink = select({{
            Label("@aspect_rules_js//js:allow_unresolved_symlinks"): True,
            "//conditions:default": False,
        }}),
    )

    # lifecycle build action
    _js_run_binary(
        name = "{{}}/lc".format(store_target_name),
        srcs = [
            "{npm_package_target}",
            ":{{}}/pkg_pre_lc".format(store_target_name),
        ],
        # js_run_binary runs in the output dir; must add "../../../" because paths are relative to the exec root
        args = [
                   "{package}",
                   "../../../$(execpath {npm_package_target})",
                   "../../../$(@D)",
               ] +
               select({{
                   Label("@aspect_rules_js//platforms/os:osx"): ["--platform=darwin"],
                   Label("@aspect_rules_js//platforms/os:linux"): ["--platform=linux"],
                   Label("@aspect_rules_js//platforms/os:windows"): ["--platform=win32"],
                   "//conditions:default": [],
               }}) +
               select({{
                   Label("@aspect_rules_js//platforms/cpu:arm64"): ["--arch=arm64"],
                   Label("@aspect_rules_js//platforms/cpu:x86_64"): ["--arch=x64"],
                   "//conditions:default": [],
               }}) +
               select({{
                   Label("@aspect_rules_js//platforms/libc:glibc"): ["--libc=glibc"],
                   Label("@aspect_rules_js//platforms/libc:musl"): ["--libc=musl"],
                   "//conditions:default": [],
               }}),
        copy_srcs_to_bin = False,
        tool = Label("@aspect_rules_js//npm/private/lifecycle:lifecycle-hooks"),
        out_dirs = ["{lifecycle_output_dir}"],
        tags = ["manual"],
        execution_requirements = {lifecycle_hooks_execution_requirements},
        mnemonic = "NpmLifecycleHook",
        progress_message = "Running lifecycle hooks on npm package {package}@{version}",
        env = {lifecycle_hooks_env},{maybe_use_default_shell_env}
    )

    # post-lifecycle npm_package
    _npm_package_internal(
        name = "{{}}/pkg_lc".format(store_target_name),
        src = ":{{}}/lc".format(store_target_name),
        package = "{package}",
        version = "{version}",
        tags = ["manual"],
    )
"""

_LINK_JS_PACKAGE_LINK_IMPORTED_STORE_TMPL = """\
# Generated npm_package_store and npm_link_package_store targets for npm package {package}@{version}
# buildifier: disable=function-docstring
def npm_link_imported_package_store(name):
    link_packages = {link_packages}
    if native.package_name() in link_packages:
        link_aliases = link_packages[native.package_name()]
    else:
        link_aliases = ["{package}"]

    link_alias = None
    for _link_alias in link_aliases:
        if name.endswith("/{{}}".format(_link_alias)):
            # longest match wins
            if not link_alias or len(_link_alias) > len(link_alias):
                link_alias = _link_alias
    if not link_alias:
        msg = "name must end with one of '/{{{{ {{link_aliases_comma_separated}} }}}}' when called from package '{package}'; recommended value(s) are 'node_modules/{{{{ {{link_aliases_comma_separated}} }}}}'".format(link_aliases_comma_separated = ", ".join(link_aliases))
        fail(msg)

    link_root_name = name[:-len("/{{}}".format(link_alias))]
    store_target_name = "{virtual_store_root}/{{}}/{virtual_store_name}".format(link_root_name)

    # terminal package store target to link
    _npm_link_package_store(
        name = name,
        package = link_alias,
        src = "//{root_package}:{{}}".format(store_target_name),
        visibility = {link_visibility},
        tags = ["manual"],
        use_declare_symlink = select({{
            Label("@aspect_rules_js//js:allow_unresolved_symlinks"): True,
            "//conditions:default": False,
        }}),{maybe_bins}
    )

    # filegroup target that provides a single file which is
    # package directory for use in $(execpath) and $(rootpath)
    native.filegroup(
        name = "{{}}/dir".format(name),
        srcs = [":{{}}".format(name)],
        output_group = "{package_directory_output_group}",
        visibility = {link_visibility},
        tags = ["manual"],
    )

    return [":{{}}".format(name)] if {public_visibility} else []
"""

_LINK_JS_PACKAGE_LINK_IMPORTED_PKG_TMPL = """\
# Generated npm_package_store and npm_link_package_store targets for npm package {package}@{version}
# buildifier: disable=function-docstring
def npm_link_imported_package(
        name = "node_modules",
        link = {link_default},
        fail_if_no_link = True):
    root_package = "{root_package}"
    link_packages = {link_packages}

    if link_packages and link != None:
        fail("link attribute cannot be specified when link_packages are set")

    is_link = (link == True) or (link == None and native.package_name() in link_packages)
    is_root = native.package_name() == root_package

    if fail_if_no_link and not is_root and not link:
        msg = "Nothing to link in bazel package '%s' for npm package npm package {package}@{version}. This is neither the root package nor a link package of this package." % native.package_name()
        fail(msg)

    link_targets = []
    scoped_targets = {{}}

    if is_link:
        link_aliases = []
        if native.package_name() in link_packages:
            link_aliases = link_packages[native.package_name()]
        if not link_aliases:
            link_aliases = ["{package}"]
        for link_alias in link_aliases:
            link_target_name = "{{}}/{{}}".format(name, link_alias)
            npm_link_imported_package_store(name = link_target_name)
            if {public_visibility}:
                link_targets.append(":{{}}".format(link_target_name))
                if len(link_alias.split("/", 1)) > 1:
                    link_scope = link_alias.split("/", 1)[0]
                    if link_scope not in scoped_targets:
                        scoped_targets[link_scope] = []
                    scoped_targets[link_scope].append(link_target_name)

    if is_root:
        npm_imported_package_store("{{}}/{package}".format(name))

    return (link_targets, scoped_targets)
"""

_BIN_MACRO_TMPL = """
def _{bin_name}_internal(name, link_root_name, **kwargs):
    store_target_name = "{virtual_store_root}/{{}}/{virtual_store_name}".format(link_root_name)
    _directory_path(
        name = "%s__entry_point" % name,
        directory = "@{link_workspace}//{root_package}:{{}}/dir".format(store_target_name),
        path = "{bin_path}",
        tags = ["manual"],
    )
    _js_binary(
        name = "%s__js_binary" % name,
        entry_point = ":%s__entry_point" % name,
        data = ["@{link_workspace}//{root_package}:{{}}".format(store_target_name)],
        include_npm = kwargs.pop("include_npm", False),
        tags = ["manual"],
    )
    _js_run_binary(
        name = name,
        tool = ":%s__js_binary" % name,
        mnemonic = kwargs.pop("mnemonic", "{bin_mnemonic}"),
        **kwargs
    )

def _{bin_name}_test_internal(name, link_root_name, **kwargs):
    store_target_name = "{virtual_store_root}/{{}}/{virtual_store_name}".format(link_root_name)
    _directory_path(
        name = "%s__entry_point" % name,
        directory = "@{link_workspace}//{root_package}:{{}}/dir".format(store_target_name),
        path = "{bin_path}",
        tags = ["manual"],
    )
    _js_test(
        name = name,
        entry_point = ":%s__entry_point" % name,
        data = kwargs.pop("data", []) + ["@{link_workspace}//{root_package}:{{}}".format(store_target_name)],
        **kwargs
    )

def _{bin_name}_binary_internal(name, link_root_name, **kwargs):
    store_target_name = "{virtual_store_root}/{{}}/{virtual_store_name}".format(link_root_name)
    _directory_path(
        name = "%s__entry_point" % name,
        directory = "@{link_workspace}//{root_package}:{{}}/dir".format(store_target_name),
        path = "{bin_path}",
        tags = ["manual"],
    )
    _js_binary(
        name = name,
        entry_point = ":%s__entry_point" % name,
        data = kwargs.pop("data", []) + ["@{link_workspace}//{root_package}:{{}}".format(store_target_name)],
        **kwargs
    )

def {bin_name}(name, **kwargs):
    _{bin_name}_internal(name, "node_modules", **kwargs)

def {bin_name}_test(name, **kwargs):
    _{bin_name}_test_internal(name, "node_modules", **kwargs)

def {bin_name}_binary(name, **kwargs):
    _{bin_name}_binary_internal(name, "node_modules", **kwargs)
"""

_JS_PACKAGE_TMPL = """
_npm_package_internal(
    name = "pkg",
    src = ":{package_src}",
    package = "{package}",
    version = "{version}",
    visibility = ["//visibility:public"],
)
"""

_BZL_LIBRARY_TMPL = \
    """
bzl_library(
    name = "{name}_bzl_library",
    srcs = ["{src}"],
    deps = [
        "@aspect_bazel_lib//lib:directory_path",
        "@aspect_rules_js//js:defs",
    ],
    visibility = ["//visibility:public"],
)"""

_TARBALL_FILENAME = "package.tgz"
_EXTRACT_TO_DIRNAME = "package"
_DEFS_BZL_FILENAME = "defs.bzl"
_PACKAGE_JSON_BZL_FILENAME = "package_json.bzl"

def _fetch_git_repository(rctx):
    if not rctx.attr.commit:
        fail("commit required if url is a git repository")

    # Adapted from git_repo helper function used by git_repository in @bazel_tools//tools/build_defs/repo:git_worker.bzl:
    # https://github.com/bazelbuild/bazel/blob/5bdd2b2ff8d6be4ecbffe82d975983129d459782/tools/build_defs/repo/git_worker.bzl#L34
    git_repo = struct(
        directory = rctx.path(_EXTRACT_TO_DIRNAME),
        shallow = "--depth=1",
        reset_ref = rctx.attr.commit,
        fetch_ref = rctx.attr.commit,
        remote = str(rctx.attr.url),
    )
    rctx.report_progress("Cloning %s of %s" % (git_repo.reset_ref, git_repo.remote))
    _git_init(rctx, git_repo)
    _git_add_origin(rctx, git_repo, rctx.attr.url)
    _git_fetch(rctx, git_repo)
    _git_reset(rctx, git_repo)
    _git_clean(rctx, git_repo)

    git_metadata_folder = git_repo.directory.get_child(".git")
    if not rctx.delete(git_metadata_folder):
        fail("Failed to delete .git folder in %s" % str(git_repo.directory))

def _is_gnu_tar(rctx):
    # We assume that any linux platform is using GNU tar.
    if repo_utils.is_linux(rctx):
        return True

    # TODO: use a hermetic tar from aspect_bazel_lib and we can drop the "if _is_gnu_tar" branch
    tar_args = ["tar", "--version"]
    result = rctx.execute(tar_args)
    if result.return_code:
        msg = "Failed to determine tar version. '{}' exited with {}: \nSTDOUT:\n{}\nSTDERR:\n{}".format(" ".join(tar_args), result.return_code, result.stdout, result.stderr)
        fail(msg)

    return "GNU tar" in result.stdout

def _download_and_extract_archive(rctx, package_json_only):
    download_url = rctx.attr.url if rctx.attr.url else utils.npm_registry_download_url(rctx.attr.package, rctx.attr.version, {}, utils.default_registry())

    auth = {}

    if rctx.attr.npm_auth_username or rctx.attr.npm_auth_password:
        if not rctx.attr.npm_auth_username:
            fail("'npm_auth_password' was provided without 'npm_auth_username'")
        if not rctx.attr.npm_auth_password:
            fail("'npm_auth_username' was provided without 'npm_auth_password'")

    auth_count = 0
    if rctx.attr.npm_auth:
        auth = {
            download_url: {
                "type": "pattern",
                "pattern": "Bearer <password>",
                "password": rctx.attr.npm_auth,
            },
        }
        auth_count += 1
    if rctx.attr.npm_auth_basic:
        auth = {
            download_url: {
                "type": "pattern",
                "pattern": "Basic <password>",
                "password": rctx.attr.npm_auth_basic,
            },
        }
        auth_count += 1
    if rctx.attr.npm_auth_username and rctx.attr.npm_auth_password:
        auth = {
            download_url: {
                "type": "basic",
                "login": rctx.attr.npm_auth_username,
                "password": rctx.attr.npm_auth_password,
            },
        }
        auth_count += 1
    if auth_count > 1:
        fail("expected only one of 'npm_auth', `npm_auth_basic` or 'npm_auth_username' and 'npm_auth_password' to be set")

    rctx.download(
        output = _TARBALL_FILENAME,
        url = download_url,
        integrity = rctx.attr.integrity,
        auth = auth,
        canonical_id = download_url,
    )

    mkdir_args = ["mkdir", "-p", _EXTRACT_TO_DIRNAME] if not repo_utils.is_windows(rctx) else ["cmd", "/c", "if not exist {extract_to_dirname} (mkdir {extract_to_dirname})".format(extract_to_dirname = _EXTRACT_TO_DIRNAME.replace("/", "\\"))]
    result = rctx.execute(mkdir_args)
    if result.return_code:
        msg = "Failed to create package directory. '{}' exited with {}: \nSTDOUT:\n{}\nSTDERR:\n{}".format(" ".join(mkdir_args), result.return_code, result.stdout, result.stderr)
        fail(msg)

    # npm packages are always published with one top-level directory inside the tarball, tho the name is not predictable
    # so we use tar here which takes a --strip-components N argument instead of rctx.download_and_extract
    tar_args = ["tar", "-xf", _TARBALL_FILENAME, "--strip-components", str(1), "-C", _EXTRACT_TO_DIRNAME, "--no-same-owner", "--no-same-permissions"]

    if _is_gnu_tar(rctx):
        # Some packages have directory permissions missing the executable bit, which prevents GNU tar from
        # extracting files into the directory. Delay permission restoration for directories until all files
        # have been extracted.
        tar_args.append("--delay-directory-restore")

    if package_json_only:
        # Try to extract package/package.json; 'package' as the root folder is the common
        # case but some npm package tarballs will use a different root folder. In this case
        # this extract call will fail and we'll fallback to extracting the full package.
        tar_args_package_json_only = tar_args[:]
        tar_args_package_json_only.append("package/package.json")
        result = rctx.execute(tar_args_package_json_only)
        if result.return_code:
            package_json_only = False

    if not package_json_only:
        result = rctx.execute(tar_args)
        if result.return_code:
            msg = "Failed to extract package tarball. '{}' exited with {}: \nSTDOUT:\n{}\nSTDERR:\n{}".format(" ".join(tar_args), result.return_code, result.stdout, result.stderr)
            fail(msg)

    if not repo_utils.is_windows(rctx):
        # Some packages have directory permissions missing executable which
        # make the directories not listable. Fix these cases in order to be able
        # to execute the copy action. https://stackoverflow.com/a/14634721
        chmod_args = ["chmod", "-R", "a+X", _EXTRACT_TO_DIRNAME]
        result = rctx.execute(chmod_args)
        if result.return_code:
            msg = "Failed to set directory listing permissions. '{}' exited with {}: \nSTDOUT:\n{}\nSTDERR:\n{}".format(" ".join(chmod_args), result.return_code, result.stdout, result.stderr)
            fail(msg)

def _npm_import_rule_impl(rctx):
    has_lifecycle_hooks = not (not rctx.attr.lifecycle_hooks) or not (not rctx.attr.custom_postinstall)
    has_patches = not (not rctx.attr.patches)

    package_src = _EXTRACT_TO_DIRNAME
    if utils.is_git_repository_url(rctx.attr.url):
        _fetch_git_repository(rctx)
    elif rctx.attr.extract_full_archive or has_patches or has_lifecycle_hooks:
        _download_and_extract_archive(rctx, package_json_only = False)
    else:
        # TODO: support tarball package_src with lifecycle hooks
        _download_and_extract_archive(rctx, package_json_only = True)
        package_src = _TARBALL_FILENAME

    # apply patches to the extracted package before reading the package.json incase
    # the patch targets the package.json itself
    patch(rctx, patch_args = rctx.attr.patch_args, patch_directory = _EXTRACT_TO_DIRNAME)

    pkg_json_path = paths.join(_EXTRACT_TO_DIRNAME, "package.json")

    pkg_json = json.decode(rctx.read(pkg_json_path))

    bins = _get_bin_entries(pkg_json, rctx.attr.package)

    generated_by_prefix = _make_generated_by_prefix(rctx.attr.package, rctx.attr.version)

    bazel_name = utils.bazel_name(rctx.attr.package, rctx.attr.version)

    rctx_files = {
        "BUILD.bazel": [
            generated_by_prefix,
            """load("@aspect_rules_js//npm/private:npm_package_internal.bzl", _npm_package_internal = "npm_package_internal")""",
        ],
    }

    rctx_files["BUILD.bazel"].append(_JS_PACKAGE_TMPL.format(
        package_src = package_src,
        package = rctx.attr.package,
        version = rctx.attr.version,
    ))

    if rctx.attr.extra_build_content:
        rctx_files["BUILD.bazel"].append("\n" + rctx.attr.extra_build_content)

    if bins:
        virtual_store_name = utils.virtual_store_name(rctx.attr.package, rctx.attr.version)
        package_name_no_scope = rctx.attr.package.rsplit("/", 1)[-1]

        for link_package in rctx.attr.link_packages.keys():
            bin_bzl = [
                generated_by_prefix,
                """load("@aspect_bazel_lib//lib:directory_path.bzl", _directory_path = "directory_path")""",
                """load("@aspect_rules_js//js:defs.bzl", _js_binary = "js_binary", _js_run_binary = "js_run_binary", _js_test = "js_test")""",
            ]
            for name in bins:
                bin_name = _sanitize_bin_name(name)
                bin_bzl.append(
                    _BIN_MACRO_TMPL.format(
                        bazel_name = bazel_name,
                        bin_name = bin_name,
                        bin_mnemonic = _mnemonic_for_bin(bin_name),
                        bin_path = bins[name],
                        link_workspace = rctx.attr.link_workspace,
                        package = rctx.attr.package,
                        root_package = rctx.attr.root_package,
                        version = rctx.attr.version,
                        virtual_store_name = virtual_store_name,
                        virtual_store_root = utils.virtual_store_root,
                    ),
                )

            bin_struct_fields = []
            for bin_name in bins:
                sanitized_bin_name = _sanitize_bin_name(bin_name)
                bin_struct_fields.append(
                    """        {bin_name} = lambda name, **kwargs: _{bin_name}_internal(name, link_root_name = link_root_name, **kwargs),
        {bin_name}_test = lambda name, **kwargs: _{bin_name}_test_internal(name, link_root_name = link_root_name, **kwargs),
        {bin_name}_binary = lambda name, **kwargs: _{bin_name}_binary_internal(name, link_root_name = link_root_name, **kwargs),
        {bin_name}_path = "{bin_path}",""".format(
                        bin_name = sanitized_bin_name,
                        bin_path = bins[bin_name],
                    ),
                )

            bin_bzl.append("""def bin_factory(link_root_name):
    # bind link_root_name using lambdas
    return struct(
{bin_struct_fields}
    )

bin = bin_factory("node_modules")
""".format(
                package = rctx.attr.package,
                version = rctx.attr.version,
                bin_struct_fields = "\n".join(bin_struct_fields),
            ))

            rctx_files[paths.join(link_package, _PACKAGE_JSON_BZL_FILENAME)] = bin_bzl

            build_file = paths.join(link_package, "BUILD.bazel")
            if build_file not in rctx_files:
                rctx_files[build_file] = [generated_by_prefix]
            if rctx.attr.generate_bzl_library_targets:
                rctx_files[build_file].append("""load("@bazel_skylib//:bzl_library.bzl", "bzl_library")""")
                rctx_files[build_file].append(_BZL_LIBRARY_TMPL.format(
                    name = link_package.split("/")[-1] or package_name_no_scope,
                    src = _PACKAGE_JSON_BZL_FILENAME,
                ))
            rctx_files[build_file].append("""exports_files(["{}", "{}"])""".format(_PACKAGE_JSON_BZL_FILENAME, package_src))

    rules_js_metadata = {}
    if rctx.attr.lifecycle_hooks:
        rules_js_metadata["lifecycle_hooks"] = ",".join(rctx.attr.lifecycle_hooks)
    if rctx.attr.custom_postinstall:
        rules_js_metadata["scripts"] = {}
        rules_js_metadata["scripts"]["custom_postinstall"] = rctx.attr.custom_postinstall

    if rules_js_metadata:
        rules_js_json_path = paths.join(_EXTRACT_TO_DIRNAME, "aspect_rules_js_metadata.json")
        rctx.file(rules_js_json_path, json.encode_indent(rules_js_metadata, indent = "  "))

    for filename, contents in rctx_files.items():
        rctx.file(filename, "\n".join(contents))

def _sanitize_bin_name(name):
    """ Sanitize a package name so we can use it in starlark function names """
    return name.replace("-", "_")

def _mnemonic_for_bin(bin_name):
    """ Sanitize a bin name so we can use it as a mnemonic.

    Creates a CamelCase version of the bin name.
    """
    bin_words = bin_name.split("_")
    return "".join([word.capitalize() for word in bin_words])

def _npm_import_links_rule_impl(rctx):
    ref_deps = {}
    lc_deps = {}
    deps = {}

    bzlmod_supported = is_bazel_6_or_greater()

    for (dep_name, dep_version) in rctx.attr.deps.items():
        store_package, store_version = utils.parse_pnpm_package_key(dep_name, dep_version)
        if dep_version.startswith("link:") or dep_version.startswith("file:"):
            dep_store_target = """"//{root_package}:{virtual_store_root}/{{}}/{virtual_store_name}".format(link_root_name)"""
        else:
            dep_store_target = """":{virtual_store_root}/{{}}/{virtual_store_name}/ref".format(link_root_name)"""
        dep_store_target = dep_store_target.format(
            root_package = rctx.attr.root_package,
            virtual_store_name = utils.virtual_store_name(store_package, store_version),
            virtual_store_root = utils.virtual_store_root,
        )
        ref_deps[dep_store_target] = ref_deps[dep_store_target] + [dep_name] if dep_store_target in ref_deps else [dep_name]

    transitive_closure_pattern = len(rctx.attr.transitive_closure) > 0
    if transitive_closure_pattern:
        # transitive closure deps pattern is used for breaking circular deps;
        # this pattern is used to break circular dependencies between 3rd
        # party npm deps; it is not recommended for 1st party deps
        for (dep_name, dep_versions) in rctx.attr.transitive_closure.items():
            for dep_version in dep_versions:
                store_package, store_version = utils.parse_pnpm_package_key(dep_name, dep_version)
                if dep_version.startswith("link:") or dep_version.startswith("file:"):
                    dep_store_target = """"//{root_package}:{virtual_store_root}/{{}}/{virtual_store_name}".format(link_root_name)"""
                    lc_dep_store_target = dep_store_target
                else:
                    dep_store_target = """":{virtual_store_root}/{{}}/{virtual_store_name}/pkg".format(link_root_name)"""
                    lc_dep_store_target = dep_store_target
                    if dep_name == rctx.attr.package and dep_version == rctx.attr.version:
                        # special case for lifecycle transitive closure deps; do not depend on
                        # the __pkg of this package as that will be the output directory
                        # of the lifecycle action
                        lc_dep_store_target = """":{virtual_store_root}/{{}}/{virtual_store_name}/pkg_pre_lc_lite".format(link_root_name)"""

                dep_store_target = dep_store_target.format(
                    root_package = rctx.attr.root_package,
                    virtual_store_name = utils.virtual_store_name(store_package, store_version),
                    virtual_store_root = utils.virtual_store_root,
                )
                lc_dep_store_target = lc_dep_store_target.format(
                    root_package = rctx.attr.root_package,
                    virtual_store_name = utils.virtual_store_name(store_package, store_version),
                    virtual_store_root = utils.virtual_store_root,
                )

                lc_deps[lc_dep_store_target] = lc_deps[lc_dep_store_target] + [dep_name] if lc_dep_store_target in lc_deps else [dep_name]
                deps[dep_store_target] = deps[dep_store_target] + [dep_name] if dep_store_target in deps else [dep_name]
    else:
        for (dep_name, dep_version) in rctx.attr.deps.items():
            store_package, store_version = utils.parse_pnpm_package_key(dep_name, dep_version)
            if dep_version.startswith("link:") or dep_version.startswith("file:"):
                dep_store_target = """"//{root_package}:{virtual_store_root}/{{}}/{virtual_store_name}".format(link_root_name)"""
            else:
                dep_store_target = """":{virtual_store_root}/{{}}/{virtual_store_name}".format(link_root_name)"""
            dep_store_target = dep_store_target.format(
                root_package = rctx.attr.root_package,
                virtual_store_name = utils.virtual_store_name(store_package, store_version),
                virtual_store_root = utils.virtual_store_root,
            )
            lc_deps[dep_store_target] = lc_deps[dep_store_target] + [dep_name] if dep_store_target in lc_deps else [dep_name]
            deps[dep_store_target] = deps[dep_store_target] + [dep_name] if dep_store_target in deps else [dep_name]

    virtual_store_name = utils.virtual_store_name(rctx.attr.package, rctx.attr.version)

    # "node_modules/{virtual_store_root}/{virtual_store_name}/node_modules/{package}"
    lifecycle_output_dir = paths.join("node_modules", utils.virtual_store_root, virtual_store_name, "node_modules", rctx.attr.package)

    # strip _links post-fix to get the repository name of the npm sources
    npm_import_sources_repo_name = rctx.name[:-len(utils.links_repo_suffix)]

    if rctx.attr.replace_package:
        npm_package_target = rctx.attr.replace_package
    else:
        npm_package_target = "{}{}//:pkg".format(
            "@@" if bzlmod_supported else "@",
            npm_import_sources_repo_name,
        )

    link_packages = {}
    for package, link_aliases in rctx.attr.link_packages.items():
        link_packages[package] = link_aliases or [rctx.attr.package]

    # collapse link aliases lists into comma separated strings
    for dep in deps.keys():
        deps[dep] = ",".join(deps[dep])
    for dep in lc_deps.keys():
        lc_deps[dep] = ",".join(lc_deps[dep])
    for dep in ref_deps.keys():
        ref_deps[dep] = ",".join(ref_deps[dep])

    lifecycle_hooks_env = {}
    for env in rctx.attr.lifecycle_hooks_env:
        key_value = env.split("=", 1)
        if len(key_value) == 2:
            lifecycle_hooks_env[key_value[0]] = key_value[1]
        else:
            msg = "lifecycle_hooks_env contains invalid key value pair '%s', required '=' separator not found" % env
            fail(msg)

    lifecycle_hooks_execution_requirements = {}
    for ec in rctx.attr.lifecycle_hooks_execution_requirements:
        lifecycle_hooks_execution_requirements[ec] = "1"

    maybe_bins = ("""
        bins = %s,""" % starlark_codegen_utils.to_dict_attr(rctx.attr.bins, 3)) if len(rctx.attr.bins) > 0 else ""

    virtual_store_name = utils.virtual_store_name(rctx.attr.package, rctx.attr.version)

    public_visibility = ("//visibility:public" in rctx.attr.package_visibility)

    npm_link_pkg_bzl_vars = dict(
        deps = starlark_codegen_utils.to_dict_attr(deps, 1, quote_key = False),
        link_default = "None" if rctx.attr.link_packages else "True",
        npm_package_target = npm_package_target,
        lc_deps = starlark_codegen_utils.to_dict_attr(lc_deps, 1, quote_key = False),
        has_lifecycle_build_target = str(rctx.attr.lifecycle_build_target),
        lifecycle_hooks_execution_requirements = starlark_codegen_utils.to_dict_attr(lifecycle_hooks_execution_requirements, 2),
        lifecycle_hooks_env = starlark_codegen_utils.to_dict_attr(lifecycle_hooks_env),
        lifecycle_output_dir = lifecycle_output_dir,
        npm_link_package_bzl = "@%s//:%s" % (rctx.name, _DEFS_BZL_FILENAME),
        link_packages = starlark_codegen_utils.to_dict_attr(link_packages, 1, quote_value = False),
        link_visibility = rctx.attr.package_visibility,
        public_visibility = str(public_visibility),
        package = rctx.attr.package,
        package_directory_output_group = utils.package_directory_output_group,
        rctx_name = rctx.name,
        ref_deps = starlark_codegen_utils.to_dict_attr(ref_deps, 1, quote_key = False),
        root_package = rctx.attr.root_package,
        transitive_closure_pattern = str(transitive_closure_pattern),
        version = rctx.attr.version,
        virtual_store_name = virtual_store_name,
        virtual_store_root = utils.virtual_store_root,
        maybe_bins = maybe_bins,
        dev = rctx.attr.dev,
        # Insert nothing when `lifecycle_hooks_use_default_shell_env` is None to remain backwards-compatible
        # with bazel-lib `run_binary` before `use_default_shell_env` was added.
        # TODO(2.0): remove support for old bazel-lib without `run_binary(use_default_shell_env)`
        maybe_use_default_shell_env = "\n        use_default_shell_env = True," if rctx.attr.lifecycle_hooks_use_default_shell_env else "",
    )

    npm_link_package_bzl = [
        tmpl.format(**npm_link_pkg_bzl_vars)
        for tmpl in [
            _LINK_JS_PACKAGE_LOADS_TMPL,
            _LINK_JS_PACKAGE_LIFECYCLE_LOADS_TMPL if rctx.attr.lifecycle_build_target else None,
            _LINK_JS_PACKAGE_TMPL,
            _LINK_JS_PACKAGE_LIFECYCLE_TMPL if rctx.attr.lifecycle_build_target else None,
            _LINK_JS_PACKAGE_LINK_IMPORTED_STORE_TMPL,
            _LINK_JS_PACKAGE_LINK_IMPORTED_PKG_TMPL,
        ]
        if tmpl
    ]

    generated_by_prefix = _make_generated_by_prefix(rctx.attr.package, rctx.attr.version)

    rctx.file(_DEFS_BZL_FILENAME, generated_by_prefix + "\n" + "\n".join(npm_link_package_bzl))

    rctx.file("BUILD.bazel", "exports_files(%s)" % starlark_codegen_utils.to_list_attr([_DEFS_BZL_FILENAME]))

_COMMON_ATTRS = {
    "link_packages": attr.string_list_dict(),
    "package": attr.string(mandatory = True),
    "root_package": attr.string(),
    "version": attr.string(mandatory = True),
}

_ATTRS_LINKS = dicts.add(_COMMON_ATTRS, {
    "bins": attr.string_dict(),
    "deps": attr.string_dict(),
    "dev": attr.bool(),
    "lifecycle_build_target": attr.bool(),
    "lifecycle_hooks_env": attr.string_list(),
    "lifecycle_hooks_execution_requirements": attr.string_list(),
    "lifecycle_hooks_use_default_shell_env": attr.bool(),
    "transitive_closure": attr.string_list_dict(),
    "package_visibility": attr.string_list(),
    "replace_package": attr.string(),
})

_ATTRS = dicts.add(_COMMON_ATTRS, {
    "commit": attr.string(),
    "custom_postinstall": attr.string(),
    "extra_build_content": attr.string(),
    "extract_full_archive": attr.bool(),
    "generate_bzl_library_targets": attr.bool(),
    "integrity": attr.string(),
    "lifecycle_hooks": attr.string_list(),
    "link_workspace": attr.string(),
    "npm_auth": attr.string(),
    "npm_auth_basic": attr.string(),
    "npm_auth_password": attr.string(),
    "npm_auth_username": attr.string(),
    "patch_args": attr.string_list(),
    "patches": attr.label_list(),
    "url": attr.string(),
})

def _get_bin_entries(pkg_json, package):
    # https://docs.npmjs.com/cli/v7/configuring-npm/package-json#bin
    bin = pkg_json.get("bin", {})
    if type(bin) != "dict":
        bin = {paths.basename(package): bin}
    return bin

def _make_generated_by_prefix(package, version):
    # empty line after bzl docstring since buildifier expects this if this file is vendored in
    return "\"@generated by @aspect_rules_js//npm/private:npm_import.bzl for npm package {package}@{version}\"\n".format(
        package = package,
        version = version,
    )

npm_import_links_lib = struct(
    implementation = _npm_import_links_rule_impl,
    attrs = _ATTRS_LINKS,
)

npm_import_lib = struct(
    implementation = _npm_import_rule_impl,
    attrs = _ATTRS,
)

npm_import_links = repository_rule(
    implementation = _npm_import_links_rule_impl,
    attrs = _ATTRS_LINKS,
)

npm_import_rule = repository_rule(
    implementation = _npm_import_rule_impl,
    attrs = _ATTRS,
)

def npm_import(
        name,
        package,
        version,
        deps = {},
        extra_build_content = "",
        transitive_closure = {},
        root_package = "",
        link_workspace = "",
        link_packages = {},
        lifecycle_hooks = [],
        lifecycle_hooks_execution_requirements = ["no-sandbox"],
        lifecycle_hooks_env = [],
        lifecycle_hooks_use_default_shell_env = False,
        integrity = "",
        url = "",
        commit = "",
        replace_package = None,
        package_visibility = ["//visibility:public"],
        patch_args = ["-p0"],
        patches = [],
        custom_postinstall = "",
        npm_auth = "",
        npm_auth_basic = "",
        npm_auth_username = "",
        npm_auth_password = "",
        bins = {},
        dev = False,
        register_copy_directory_toolchains = True,
        register_copy_to_directory_toolchains = True,
        # TODO(2.0): remove run_lifecycle_hooks from npm_import
        run_lifecycle_hooks = None,
        # TODO(2.0): remove lifecycle_hooks_no_sandbox from npm_import
        lifecycle_hooks_no_sandbox = None,
        **kwargs):
    """Import a single npm package into Bazel.

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

    > This is similar to Bazel rules in other ecosystems named "_import" like
    > `apple_bundle_import`, `scala_import`, `java_import`, and `py_import`.
    > `go_repository` is also a model for this rule.

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

    Read more about the downloader config: <https://blog.aspect.dev/configuring-bazels-downloader>

    [UrlRewriterConfig]: https://github.com/bazelbuild/bazel/blob/4.2.1/src/main/java/com/google/devtools/build/lib/bazel/repository/downloader/UrlRewriterConfig.java#L66

    Args:
        name: Name for this repository rule

        package: Name of the npm package, such as `acorn` or `@types/node`

        version: Version of the npm package, such as `8.4.0`

        deps: A dict other npm packages this one depends on where the key is the package name and value is the version

        transitive_closure: A dict all npm packages this one depends on directly or transitively where the key is the
            package name and value is a list of version(s) depended on in the closure.

        root_package: The root package where the node_modules virtual store is linked to.
            Typically this is the package that the pnpm-lock.yaml file is located when using `npm_translate_lock`.

        link_workspace: The workspace name where links will be created for this package.

            This is typically set in rule sets and libraries that are to be consumed as external repositories so
            links are created in the external repository and not the user workspace.

            Can be left unspecified if the link workspace is the user workspace.

        link_packages: Dict of paths where links may be created at for this package to
            a list of link aliases to link as in each package. If aliases are an
            empty list this indicates to link as the package name.

            Defaults to {} which indicates that links may be created in any package as specified by
            the `direct` attribute of the generated npm_link_package.

        lifecycle_hooks: List of lifecycle hook `package.json` scripts to run for this package if they exist.

        lifecycle_hooks_env: Environment variables set for the lifecycle hooks action for this npm
            package if there is one.

            Environment variables are defined by providing an array of "key=value" entries.

            For example:

            ```
            lifecycle_hooks_env: ["PREBULT_BINARY=https://downloadurl"],
            ```

        lifecycle_hooks_execution_requirements: Execution requirements when running the lifecycle hooks.

            For example:

            ```
            lifecycle_hooks_execution_requirements: ["no-sandbox', "requires-network"]
            ```

            This defaults to ["no-sandbox"] to limit the overhead of sandbox creation and copying the output
            TreeArtifact out of the sandbox.

        lifecycle_hooks_use_default_shell_env: If True, the `use_default_shell_env` attribute of lifecycle hook
            actions is set to True.

            See [use_default_shell_env](https://bazel.build/rules/lib/builtins/actions#run.use_default_shell_env)

            Note: [--incompatible_merge_fixed_and_default_shell_env](https://bazel.build/reference/command-line-reference#flag--incompatible_merge_fixed_and_default_shell_env)
            is often required and not enabled by default in Bazel < 7.0.0.

            This defaults to False reduce the negative effects of `use_default_shell_env`. Requires bazel-lib >= 2.4.2.

        integrity: Expected checksum of the file downloaded, in Subresource Integrity format.
            This must match the checksum of the file downloaded.

            This is the same as appears in the pnpm-lock.yaml, yarn.lock or package-lock.json file.

            It is a security risk to omit the checksum as remote files can change.

            At best omitting this field will make your build non-hermetic.

            It is optional to make development easier but should be set before shipping.

        url: Optional url for this package. If unset, a default npm registry url is generated from
            the package name and version.

            May start with `git+ssh://` or `git+https://` to indicate a git repository. For example,

            ```
            git+ssh://git@github.com/org/repo.git
            ```

            If url is configured as a git repository, the commit attribute must be set to the
            desired commit.

        commit: Specific commit to be checked out if url is a git repository.

        replace_package: Use the specified npm_package target when linking instead of the fetched sources for this npm package.

            The injected npm_package target may optionally contribute transitive npm package dependencies on top
            of the transitive dependencies specified in the pnpm lock file for the same package, however, these
            transitive dependencies must not collide with pnpm lock specified transitive dependencies.

            Any patches specified for this package will be not applied to the injected npm_package target. They
            will be applied, however, to the fetches sources so they can still be useful for patching the fetched
            `package.json` file, which is used to determine the generated bin entries for the package.

            NB: lifecycle hooks and custom_postinstall scripts, if implicitly or explicitly enabled, will be run on
            the injected npm_package. These may be disabled explicitly using the `lifecycle_hooks` attribute.

        package_visibility: Visibility of generated node_module link targets.

        patch_args: Arguments to pass to the patch tool.

            `-p1` will usually be needed for patches generated by git.

        patches: Patch files to apply onto the downloaded npm package.

        custom_postinstall: Custom string postinstall script to run on the installed npm package. Runs after any
            existing lifecycle hooks if `run_lifecycle_hooks` is True.

        npm_auth: Auth token to authenticate with npm. When using Bearer authentication.

        npm_auth_basic: Auth token to authenticate with npm. When using Basic authentication.

            This is typically the base64 encoded string "username:password".

        npm_auth_username: Auth username to authenticate with npm. When using Basic authentication.

        npm_auth_password: Auth password to authenticate with npm. When using Basic authentication.

        extra_build_content: Additional content to append on the generated BUILD file at the root of
            the created repository, either as a string or a list of lines similar to
            <https://github.com/bazelbuild/bazel-skylib/blob/main/docs/write_file_doc.md>.

        bins: Dictionary of `node_modules/.bin` binary files to create mapped to their node entry points.

            This is typically derived from the "bin" attribute in the package.json
            file of the npm package being linked.

            For example:

            ```
            bins = {
                "foo": "./foo.js",
                "bar": "./bar.js",
            }
            ```

            In the future, this field may be automatically populated by npm_translate_lock
            from information in the pnpm lock file. That feature is currently blocked on
            https://github.com/pnpm/pnpm/issues/5131.

        dev: Whether this npm package is a dev dependency

        register_copy_directory_toolchains: if True, `@aspect_bazel_lib//lib:repositories.bzl` `register_copy_directory_toolchains()` is called if the toolchain is not already registered

        register_copy_to_directory_toolchains: if True, `@aspect_bazel_lib//lib:repositories.bzl` `register_copy_to_directory_toolchains()` is called if the toolchain is not already registered

        run_lifecycle_hooks: If True, runs `preinstall`, `install`, `postinstall` and 'prepare' lifecycle hooks declared
            in this package.

            Deprecated. Use `lifecycle_hooks` instead.

        lifecycle_hooks_no_sandbox: If True, adds "no-sandbox" to `lifecycle_hooks_execution_requirements`.

            Deprecated. Add "no-sandbox" to `lifecycle_hooks_execution_requirements` instead.

        **kwargs: Internal use only
    """

    # TODO(2.0): move this to a new required rules_js_repositories() WORKSPACE function
    if register_copy_directory_toolchains and not native.existing_rule("copy_directory_toolchains"):
        _register_copy_directory_toolchains()
    if register_copy_to_directory_toolchains and not native.existing_rule("copy_to_directory_toolchains"):
        _register_copy_to_directory_toolchains()

    generate_bzl_library_targets = kwargs.pop("generate_bzl_library_targets", None)
    extract_full_archive = kwargs.pop("extract_full_archive", None)
    if len(kwargs):
        msg = "Invalid npm_import parameter '{}'".format(kwargs.keys()[0])
        fail(msg)

    if lifecycle_hooks and run_lifecycle_hooks:
        fail("Expected only one of lifecycle_hooks or run_lifecycle_hooks")
    if run_lifecycle_hooks:
        lifecycle_hooks = ["preinstall", "install", "postinstall"]

    # By convention, the `{name}` repository contains the actual npm
    # package sources downloaded from the registry and extracted
    npm_import_rule(
        name = name,
        package = package,
        version = version,
        root_package = root_package,
        link_workspace = link_workspace,
        link_packages = link_packages,
        integrity = integrity,
        url = url,
        commit = commit,
        patch_args = patch_args,
        patches = patches,
        custom_postinstall = custom_postinstall,
        npm_auth = npm_auth,
        npm_auth_basic = npm_auth_basic,
        npm_auth_username = npm_auth_username,
        npm_auth_password = npm_auth_password,
        lifecycle_hooks = lifecycle_hooks,
        extra_build_content = (
            extra_build_content if type(extra_build_content) == "string" else "\n".join(extra_build_content)
        ),
        generate_bzl_library_targets = generate_bzl_library_targets,
        extract_full_archive = extract_full_archive,
    )

    if lifecycle_hooks_no_sandbox:
        if "no-sandbox" not in lifecycle_hooks_execution_requirements:
            lifecycle_hooks_execution_requirements.append("no-sandbox")

    has_custom_postinstall = not (not custom_postinstall)
    has_lifecycle_hooks = not (not lifecycle_hooks)

    # By convention, the `{name}{utils.links_repo_suffix}` repository contains the generated
    # code to link this npm package into one or more node_modules trees
    npm_import_links(
        name = "{}{}".format(name, utils.links_repo_suffix),
        package = package,
        version = version,
        dev = dev,
        root_package = root_package,
        link_packages = link_packages,
        deps = deps,
        transitive_closure = transitive_closure,
        lifecycle_build_target = has_lifecycle_hooks or has_custom_postinstall,
        lifecycle_hooks_env = lifecycle_hooks_env,
        lifecycle_hooks_execution_requirements = lifecycle_hooks_execution_requirements,
        lifecycle_hooks_use_default_shell_env = lifecycle_hooks_use_default_shell_env,
        bins = bins,
        package_visibility = package_visibility,
        replace_package = replace_package,
    )
