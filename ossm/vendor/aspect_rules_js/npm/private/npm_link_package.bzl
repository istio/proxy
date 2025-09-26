"npm_link_package rule"

load(":utils.bzl", "utils")
load(":npm_package_store.bzl", "npm_package_store")
load(":npm_link_package_store.bzl", "npm_link_package_store")

def npm_link_package(
        name,
        root_package = "",
        link = True,
        src = None,
        deps = {},
        fail_if_no_link = True,
        auto_manual = True,
        visibility = ["//visibility:public"],
        **kwargs):
    """"Links an npm package to node_modules if link is True.

    When called at the root_package, a virtual store target is generated named `link__{bazelified_name}__store`.

    When linking, a `{name}` target is generated which consists of the `node_modules/<package>` symlink and transitively
    its virtual store link and the virtual store links of the transitive closure of deps.

    When linking, `{name}/dir` filegroup is also generated that refers to a directory artifact can be used to access
    the package directory for creating entry points or accessing files in the package.

    Args:
        name: The name of the link target to create if `link` is True.
            For first-party deps linked across a workspace, the name must match in all packages
            being linked as it is used to derive the virtual store link target name.
        root_package: the root package where the node_modules virtual store is linked to
        link: whether or not to link in this package
            If false, only the npm_package_store target will be created _if_ this is called in the `root_package`.
        src: the npm_package target to link; may only to be specified when linking in the root package
        deps: list of npm_package_store; may only to be specified when linking in the root package
        fail_if_no_link: whether or not to fail if this is called in a package that is not the root package and `link` is False
        auto_manual: whether or not to automatically add a manual tag to the generated targets
            Links tagged "manual" dy default is desirable so that they are not built by `bazel build ...` if they
            are unused downstream. For 3rd party deps, this is particularly important so that 3rd party deps are
            not fetched at all unless they are used.
        visibility: the visibility of the link target
        **kwargs: see attributes of npm_package_store rule

    Returns:
        Label of the npm_link_package_store if created, else None
    """
    is_root = native.package_name() == root_package

    if fail_if_no_link and not is_root and not link:
        msg = "Nothing to link in bazel package '{bazel_package}' for {name}. This is neither the root package nor a link package.".format(
            bazel_package = native.package_name(),
            name = name,
        )
        fail(msg)

    if deps and not is_root:
        msg = "deps may only be specified when linking in the root package '{}'".format(root_package)
        fail(msg)

    if src and not is_root:
        msg = "src may only be specified when linking in the root package '{}'".format(root_package)
        fail(msg)

    store_target_name = "{virtual_store_root}/{name}".format(
        name = name,
        virtual_store_root = utils.virtual_store_root,
    )

    tags = kwargs.pop("tags", [])
    if auto_manual and "manual" not in tags:
        tags.append("manual")

    if is_root:
        # link the virtual store when linking at the root
        npm_package_store(
            name = store_target_name,
            src = src,
            deps = deps,
            visibility = ["//visibility:public"],
            tags = tags,
            use_declare_symlink = select({
                Label("@aspect_rules_js//js:allow_unresolved_symlinks"): True,
                "//conditions:default": False,
            }),
            **kwargs
        )

    link_target = None
    if link:
        # create the npm package store for this package
        npm_link_package_store(
            name = name,
            src = "//{root_package}:{store_target_name}".format(
                root_package = root_package,
                store_target_name = store_target_name,
            ),
            tags = tags,
            visibility = visibility,
            use_declare_symlink = select({
                Label("@aspect_rules_js//js:allow_unresolved_symlinks"): True,
                "//conditions:default": False,
            }),
        )
        link_target = ":{}".format(name)

        # filegroup target that provides a single file which is
        # package directory for use in $(execpath) and $(rootpath)
        native.filegroup(
            name = "{}/dir".format(name),
            srcs = [link_target],
            output_group = utils.package_directory_output_group,
            tags = tags,
            visibility = visibility,
        )

    return link_target
