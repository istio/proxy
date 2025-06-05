"Module extensions for use with bzlmod"

load(
    "@aspect_bazel_lib//lib:repositories.bzl",
    "DEFAULT_BATS_CORE_VERSION",
    "DEFAULT_BATS_REPOSITORY",
    "DEFAULT_COPY_DIRECTORY_REPOSITORY",
    "DEFAULT_COPY_TO_DIRECTORY_REPOSITORY",
    "DEFAULT_COREUTILS_REPOSITORY",
    "DEFAULT_COREUTILS_VERSION",
    "DEFAULT_EXPAND_TEMPLATE_REPOSITORY",
    "DEFAULT_JQ_REPOSITORY",
    "DEFAULT_JQ_VERSION",
    "DEFAULT_TAR_REPOSITORY",
    "DEFAULT_YQ_REPOSITORY",
    "DEFAULT_YQ_VERSION",
    "DEFAULT_ZSTD_REPOSITORY",
    "register_bats_toolchains",
    "register_copy_directory_toolchains",
    "register_copy_to_directory_toolchains",
    "register_coreutils_toolchains",
    "register_expand_template_toolchains",
    "register_jq_toolchains",
    "register_tar_toolchains",
    "register_yq_toolchains",
    "register_zstd_toolchains",
)
load("@bazel_features//:features.bzl", "bazel_features")
load("//lib/private:extension_utils.bzl", "extension_utils")
load("//lib/private:host_repo.bzl", "host_repo")

def _host_extension_impl(mctx):
    create_host_repo = False
    for module in mctx.modules:
        if len(module.tags.host) > 0:
            create_host_repo = True

    if create_host_repo:
        host_repo(name = "aspect_bazel_lib_host")

host = module_extension(
    implementation = _host_extension_impl,
    tag_classes = {
        "host": tag_class(attrs = {}),
    },
)

def _toolchains_extension_impl(mctx):
    extension_utils.toolchain_repos_bfs(
        mctx = mctx,
        get_tag_fn = lambda tags: tags.copy_directory,
        toolchain_name = "copy_directory",
        toolchain_repos_fn = lambda name, version: register_copy_directory_toolchains(name = name, register = False),
        get_version_fn = lambda attr: None,
    )

    extension_utils.toolchain_repos_bfs(
        mctx = mctx,
        get_tag_fn = lambda tags: tags.copy_to_directory,
        toolchain_name = "copy_to_directory",
        toolchain_repos_fn = lambda name, version: register_copy_to_directory_toolchains(name = name, register = False),
        get_version_fn = lambda attr: None,
    )

    extension_utils.toolchain_repos_bfs(
        mctx = mctx,
        get_tag_fn = lambda tags: tags.jq,
        toolchain_name = "jq",
        toolchain_repos_fn = lambda name, version: register_jq_toolchains(name = name, version = version, register = False),
    )

    extension_utils.toolchain_repos_bfs(
        mctx = mctx,
        get_tag_fn = lambda tags: tags.yq,
        toolchain_name = "yq",
        toolchain_repos_fn = lambda name, version: register_yq_toolchains(name = name, version = version, register = False),
    )

    extension_utils.toolchain_repos_bfs(
        mctx = mctx,
        get_tag_fn = lambda tags: tags.coreutils,
        toolchain_name = "coreutils",
        toolchain_repos_fn = lambda name, version: register_coreutils_toolchains(name = name, version = version, register = False),
    )

    extension_utils.toolchain_repos_bfs(
        mctx = mctx,
        get_tag_fn = lambda tags: tags.tar,
        toolchain_name = "tar",
        default_repository = DEFAULT_TAR_REPOSITORY,
        toolchain_repos_fn = lambda name, version: register_tar_toolchains(name = name, register = False),
        get_version_fn = lambda attr: None,
    )

    extension_utils.toolchain_repos_bfs(
        mctx = mctx,
        get_tag_fn = lambda tags: tags.zstd,
        toolchain_name = "zstd",
        default_repository = DEFAULT_ZSTD_REPOSITORY,
        toolchain_repos_fn = lambda name, version: register_zstd_toolchains(name = name, register = False),
        get_version_fn = lambda attr: None,
    )

    extension_utils.toolchain_repos_bfs(
        mctx = mctx,
        get_tag_fn = lambda tags: tags.expand_template,
        toolchain_name = "expand_template",
        toolchain_repos_fn = lambda name, version: register_expand_template_toolchains(name = name, register = False),
        get_version_fn = lambda attr: None,
    )

    extension_utils.toolchain_repos_bfs(
        mctx = mctx,
        get_tag_fn = lambda tags: tags.bats,
        toolchain_name = "bats",
        default_repository = DEFAULT_BATS_REPOSITORY,
        toolchain_repos_fn = lambda name, version: register_bats_toolchains(name = name, core_version = version, register = False),
        get_version_fn = lambda attr: attr.core_version,
    )

    if bazel_features.external_deps.extension_metadata_has_reproducible:
        return mctx.extension_metadata(reproducible = True)

    return mctx.extension_metadata()

toolchains = module_extension(
    implementation = _toolchains_extension_impl,
    tag_classes = {
        "copy_directory": tag_class(attrs = {"name": attr.string(default = DEFAULT_COPY_DIRECTORY_REPOSITORY)}),
        "copy_to_directory": tag_class(attrs = {"name": attr.string(default = DEFAULT_COPY_TO_DIRECTORY_REPOSITORY)}),
        "jq": tag_class(attrs = {"name": attr.string(default = DEFAULT_JQ_REPOSITORY), "version": attr.string(default = DEFAULT_JQ_VERSION)}),
        "yq": tag_class(attrs = {"name": attr.string(default = DEFAULT_YQ_REPOSITORY), "version": attr.string(default = DEFAULT_YQ_VERSION)}),
        "coreutils": tag_class(attrs = {"name": attr.string(default = DEFAULT_COREUTILS_REPOSITORY), "version": attr.string(default = DEFAULT_COREUTILS_VERSION)}),
        "tar": tag_class(attrs = {"name": attr.string(default = DEFAULT_TAR_REPOSITORY)}),
        "zstd": tag_class(attrs = {"name": attr.string(default = DEFAULT_ZSTD_REPOSITORY)}),
        "expand_template": tag_class(attrs = {"name": attr.string(default = DEFAULT_EXPAND_TEMPLATE_REPOSITORY)}),
        "bats": tag_class(attrs = {
            "name": attr.string(default = DEFAULT_BATS_REPOSITORY),
            "core_version": attr.string(default = DEFAULT_BATS_CORE_VERSION),
        }),
    },
)
