"""Utility functions for bzlmod extensions"""

def _toolchain_repos_bfs(mctx, get_tag_fn, toolchain_name, toolchain_repos_fn, default_repository = None, get_name_fn = None, get_version_fn = None):
    """Create toolchain repositories from bzlmod extensions using a breadth-first resolution strategy.

    Toolchains are assumed to have a "default" or canonical repository name so that across
    all invocations of the module extension with that name only a single toolchain repository
    is created. As such, it is recommended to default the toolchain name in the extension's
    tag class attributes so that diverging from the canonical name is a special case.

    The resolved toolchain version will be the one invoked closest to the root module, following
    Bazel's breadth-first ordering of modules in the dependency graph.

    For example, given the module extension usage in a MODULE file:

    ```starlark
    ext = use_extension("@my_lib//lib:extensions.bzl", "ext")

    ext.foo_toolchain(version = "1.2.3") # Default `name = "foo"`

    use_repo(ext, "foo")

    register_toolchains(
        "@foo//:all",
    )
    ```

    This macro would be used in the module extension implementation as follows:

    ```starlark
    extension_utils.toolchain_repos(
        mctx = mctx,
        get_tag_fn = lambda tags: tags.foo_toolchain,
        toolchain_name = "foo",
        toolchain_repos_fn = lambda name, version: register_foo_toolchains(name = name, register = False),
        get_version_fn = lambda attr: None,
    )
    ```

    Where `register_foo_toolchains` is a typical WORKSPACE macro used to register
    the foo toolchain for a particular version, minus the actual registration step
    which is done separately in the MODULE file.

    This macro enforces that only root MODULEs may use a different name for the toolchain
    in case several versions of the toolchain repository is desired.

    Args:
        mctx: The module context
        get_tag_fn: A function that takes in `module.tags` and returns the tag used for the toolchain.
          For example, `tag: lambda tags: tags.foo_toolchain`. This is required because `foo_toolchain`
          cannot be accessed as a simple string key from `module.tags`.
        toolchain_name: Name of the toolchain to use in error messages
        toolchain_repos_fn: A function that takes (name, version) and creates a toolchain repository. This lambda
          should call a typical reposotiory rule to create toolchains.
        default_repository: Default name of the toolchain repository to pass to the repos_fn.
          By default, it equals `toolchain_name`.
        get_name_fn: A function that extracts the module name from the toolchain tag's attributes. Defaults
          to grabbing the `name` attribute.
        get_version_fn: A function that extracts the module version from the a tag's attributes. Defaults
          to grabbing the `version` attribute. Override this to a lambda that returns `None` if
          version isn't used as an attribute.
    """
    if default_repository == None:
        default_repository = toolchain_name

    if get_name_fn == None:
        get_name_fn = lambda attr: attr.name
    if get_version_fn == None:
        get_version_fn = lambda attr: attr.version

    registrations = {}
    for mod in mctx.modules:
        for attr in get_tag_fn(mod.tags):
            name = get_name_fn(attr)
            version = get_version_fn(attr)
            if name != default_repository and not mod.is_root:
                fail("Only the root module may provide a name for the {} toolchain.".format(toolchain_name))

            if name in registrations.keys():
                if name == default_repository:
                    # Prioritize the root-most registration of the default toolchain version and
                    # ignore any further registrations (modules are processed breadth-first)
                    continue
                if version == registrations[name]:
                    # No problem to register a matching toolchain twice
                    continue
                fail("Multiple conflicting {} toolchains declared for name {} ({} and {})".format(
                    toolchain_name,
                    name,
                    version,
                    registrations[name],
                ))
            else:
                registrations[name] = version

    for name, version in registrations.items():
        toolchain_repos_fn(
            name = name,
            version = version,
        )

extension_utils = struct(
    toolchain_repos_bfs = _toolchain_repos_bfs,
)
