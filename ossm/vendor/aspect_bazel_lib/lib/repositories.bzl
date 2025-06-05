"Macros for loading dependencies and registering toolchains"

load("//lib:utils.bzl", http_archive = "maybe_http_archive")
load("//lib/private:bats_toolchain.bzl", "BATS_ASSERT_VERSIONS", "BATS_CORE_TEMPLATE", "BATS_CORE_VERSIONS", "BATS_FILE_VERSIONS", "BATS_LIBRARY_TEMPLATE", "BATS_SUPPORT_VERSIONS")
load("//lib/private:copy_directory_toolchain.bzl", "COPY_DIRECTORY_PLATFORMS", "copy_directory_platform_repo", "copy_directory_toolchains_repo")
load("//lib/private:copy_to_directory_toolchain.bzl", "COPY_TO_DIRECTORY_PLATFORMS", "copy_to_directory_platform_repo", "copy_to_directory_toolchains_repo")
load("//lib/private:coreutils_toolchain.bzl", "COREUTILS_PLATFORMS", "coreutils_platform_repo", "coreutils_toolchains_repo", _DEFAULT_COREUTILS_VERSION = "DEFAULT_COREUTILS_VERSION")
load("//lib/private:expand_template_toolchain.bzl", "EXPAND_TEMPLATE_PLATFORMS", "expand_template_platform_repo", "expand_template_toolchains_repo")
load("//lib/private:jq_toolchain.bzl", "JQ_PLATFORMS", "jq_host_alias_repo", "jq_platform_repo", "jq_toolchains_repo", _DEFAULT_JQ_VERSION = "DEFAULT_JQ_VERSION")
load("//lib/private:source_toolchains_repo.bzl", "source_toolchains_repo")
load("//lib/private:tar_toolchain.bzl", "BSDTAR_PLATFORMS", "bsdtar_binary_repo", "tar_toolchains_repo")
load("//lib/private:yq_toolchain.bzl", "YQ_PLATFORMS", "yq_host_alias_repo", "yq_platform_repo", "yq_toolchains_repo", _DEFAULT_YQ_VERSION = "DEFAULT_YQ_VERSION")
load("//lib/private:zstd_toolchain.bzl", "ZSTD_PLATFORMS", "zstd_binary_repo", "zstd_toolchains_repo")
load("//tools:version.bzl", "IS_PRERELEASE")

# buildifier: disable=unnamed-macro
def aspect_bazel_lib_dependencies():
    "Load dependencies required by aspect rules"

    http_archive(
        name = "bazel_skylib",
        sha256 = "bc283cdfcd526a52c3201279cda4bc298652efa898b10b4db0837dc51652756f",
        urls = [
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.7.1/bazel-skylib-1.7.1.tar.gz",
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.7.1/bazel-skylib-1.7.1.tar.gz",
        ],
    )

DEFAULT_JQ_REPOSITORY = "jq"
DEFAULT_JQ_VERSION = _DEFAULT_JQ_VERSION

def register_jq_toolchains(name = DEFAULT_JQ_REPOSITORY, version = DEFAULT_JQ_VERSION, register = True):
    """Registers jq toolchain and repositories

    Args:
        name: override the prefix for the generated toolchain repositories
        version: the version of jq to execute (see https://github.com/stedolan/jq/releases)
        register: whether to call through to native.register_toolchains.
            Should be True for WORKSPACE users, but false when used under bzlmod extension
    """
    for [platform, _] in JQ_PLATFORMS.items():
        jq_platform_repo(
            name = "%s_%s" % (name, platform),
            platform = platform,
            version = version,
        )
        if register:
            native.register_toolchains("@%s_toolchains//:%s_toolchain" % (name, platform))

    jq_host_alias_repo(name = name)

    jq_toolchains_repo(
        name = "%s_toolchains" % name,
        user_repository_name = name,
    )

DEFAULT_YQ_REPOSITORY = "yq"
DEFAULT_YQ_VERSION = _DEFAULT_YQ_VERSION

def register_yq_toolchains(name = DEFAULT_YQ_REPOSITORY, version = DEFAULT_YQ_VERSION, register = True):
    """Registers yq toolchain and repositories

    Args:
        name: override the prefix for the generated toolchain repositories
        version: the version of yq to execute (see https://github.com/mikefarah/yq/releases)
        register: whether to call through to native.register_toolchains.
            Should be True for WORKSPACE users, but false when used under bzlmod extension
    """
    for [platform, _] in YQ_PLATFORMS.items():
        yq_platform_repo(
            name = "%s_%s" % (name, platform),
            platform = platform,
            version = version,
        )
        if register:
            native.register_toolchains("@%s_toolchains//:%s_toolchain" % (name, platform))

    yq_host_alias_repo(name = name)

    yq_toolchains_repo(
        name = "%s_toolchains" % name,
        user_repository_name = name,
    )

DEFAULT_TAR_REPOSITORY = "bsd_tar"

def register_tar_toolchains(name = DEFAULT_TAR_REPOSITORY, register = True):
    """Registers bsdtar toolchain and repositories

    Args:
        name: override the prefix for the generated toolchain repositories
        register: whether to call through to native.register_toolchains.
            Should be True for WORKSPACE users, but false when used under bzlmod extension
    """
    for [platform, _] in BSDTAR_PLATFORMS.items():
        bsdtar_binary_repo(
            name = "%s_%s" % (name, platform),
            platform = platform,
        )
        if register:
            native.register_toolchains("@%s_toolchains//:%s_toolchain" % (name, platform))

    tar_toolchains_repo(
        name = "%s_toolchains" % name,
        user_repository_name = name,
    )

DEFAULT_ZSTD_REPOSITORY = "zstd"

def register_zstd_toolchains(name = DEFAULT_ZSTD_REPOSITORY, register = True):
    """Registers zstd toolchain and repositories

    Args:
        name: override the prefix for the generated toolchain repositories
        register: whether to call through to native.register_toolchains.
            Should be True for WORKSPACE users, but false when used under bzlmod extension
    """
    for [platform, _] in ZSTD_PLATFORMS.items():
        zstd_binary_repo(
            name = "%s_%s" % (name, platform),
            platform = platform,
        )
        if register:
            native.register_toolchains("@%s_toolchains//:%s_toolchain" % (name, platform))

    zstd_toolchains_repo(
        name = "%s_toolchains" % name,
        user_repository_name = name,
    )

DEFAULT_BATS_REPOSITORY = "bats"

DEFAULT_BATS_CORE_VERSION = "v1.10.0"
DEFAULT_BATS_SUPPORT_VERSION = "v0.3.0"
DEFAULT_BATS_ASSERT_VERSION = "v2.1.0"
DEFAULT_BATS_FILE_VERSION = "v0.4.0"

def register_bats_toolchains(
        name = DEFAULT_BATS_REPOSITORY,
        core_version = DEFAULT_BATS_CORE_VERSION,
        support_version = DEFAULT_BATS_SUPPORT_VERSION,
        assert_version = DEFAULT_BATS_ASSERT_VERSION,
        file_version = DEFAULT_BATS_FILE_VERSION,
        libraries = [],
        register = True):
    """Registers bats toolchain and repositories

    Args:
        name: override the prefix for the generated toolchain repositories
        core_version: bats-core version to use
        support_version: bats-support version to use
        assert_version: bats-assert version to use
        file_version: bats-file version to use
        libraries: additional labels for libraries
        register: whether to call through to native.register_toolchains.
            Should be True for WORKSPACE users, but false when used under bzlmod extension
    """

    http_archive(
        name = "%s_support" % name,
        sha256 = BATS_SUPPORT_VERSIONS[support_version],
        urls = [
            "https://github.com/bats-core/bats-support/archive/{}.tar.gz".format(support_version),
        ],
        strip_prefix = "bats-support-{}".format(support_version.removeprefix("v")),
        build_file_content = BATS_LIBRARY_TEMPLATE.format(name = "support"),
    )

    http_archive(
        name = "%s_assert" % name,
        sha256 = BATS_ASSERT_VERSIONS[assert_version],
        urls = [
            "https://github.com/bats-core/bats-assert/archive/{}.tar.gz".format(assert_version),
        ],
        strip_prefix = "bats-assert-{}".format(assert_version.removeprefix("v")),
        build_file_content = BATS_LIBRARY_TEMPLATE.format(name = "assert"),
    )

    http_archive(
        name = "%s_file" % name,
        sha256 = BATS_FILE_VERSIONS[file_version],
        urls = [
            "https://github.com/bats-core/bats-file/archive/{}.tar.gz".format(file_version),
        ],
        strip_prefix = "bats-file-{}".format(file_version.removeprefix("v")),
        build_file_content = BATS_LIBRARY_TEMPLATE.format(name = "file"),
    )

    http_archive(
        name = "%s_toolchains" % name,
        sha256 = BATS_CORE_VERSIONS[core_version],
        urls = [
            "https://github.com/bats-core/bats-core/archive/{}.tar.gz".format(core_version),
        ],
        strip_prefix = "bats-core-{}".format(core_version.removeprefix("v")),
        build_file_content = BATS_CORE_TEMPLATE.format(libraries = [
            "@%s_support//:support" % name,
            "@%s_assert//:assert" % name,
            "@%s_file//:file" % name,
        ] + libraries),
    )

    if register:
        native.register_toolchains("@%s_toolchains//:bats_toolchain" % name)

DEFAULT_COREUTILS_REPOSITORY = "coreutils"
DEFAULT_COREUTILS_VERSION = _DEFAULT_COREUTILS_VERSION

def register_coreutils_toolchains(name = DEFAULT_COREUTILS_REPOSITORY, version = DEFAULT_COREUTILS_VERSION, register = True):
    """Registers coreutils toolchain and repositories

    Args:
        name: override the prefix for the generated toolchain repositories
        version: the version of coreutils to execute (see https://github.com/uutils/coreutils/releases)
        register: whether to call through to native.register_toolchains.
            Should be True for WORKSPACE users, but false when used under bzlmod extension
    """
    for [platform, _] in COREUTILS_PLATFORMS.items():
        coreutils_platform_repo(
            name = "%s_%s" % (name, platform),
            platform = platform,
            version = version,
        )
        if register:
            native.register_toolchains("@%s_toolchains//:%s_toolchain" % (name, platform))

    coreutils_toolchains_repo(
        name = "%s_toolchains" % name,
        user_repository_name = name,
    )

DEFAULT_COPY_DIRECTORY_REPOSITORY = "copy_directory"

def register_copy_directory_toolchains(name = DEFAULT_COPY_DIRECTORY_REPOSITORY, register = True):
    """Registers copy_directory toolchain and repositories

    Args:
        name: override the prefix for the generated toolchain repositories
        register: whether to call through to native.register_toolchains.
            Should be True for WORKSPACE users, but false when used under bzlmod extension
    """
    if IS_PRERELEASE:
        source_toolchains_repo(
            name = "%s_toolchains" % name,
            toolchain_type = "@aspect_bazel_lib//lib:copy_directory_toolchain_type",
            toolchain_rule_load_from = "@aspect_bazel_lib//lib/private:copy_directory_toolchain.bzl",
            toolchain_rule = "copy_directory_toolchain",
            binary = "@aspect_bazel_lib//tools/copy_directory",
        )
        if register:
            native.register_toolchains("@%s_toolchains//:toolchain" % name)
        return

    for [platform, _] in COPY_DIRECTORY_PLATFORMS.items():
        copy_directory_platform_repo(
            name = "%s_%s" % (name, platform),
            platform = platform,
        )
        if register:
            native.register_toolchains("@%s_toolchains//:%s_toolchain" % (name, platform))

    copy_directory_toolchains_repo(
        name = "%s_toolchains" % name,
        user_repository_name = name,
    )

DEFAULT_COPY_TO_DIRECTORY_REPOSITORY = "copy_to_directory"

def register_copy_to_directory_toolchains(name = DEFAULT_COPY_TO_DIRECTORY_REPOSITORY, register = True):
    """Registers copy_to_directory toolchain and repositories

    Args:
        name: override the prefix for the generated toolchain repositories
        register: whether to call through to native.register_toolchains.
            Should be True for WORKSPACE users, but false when used under bzlmod extension
    """
    if IS_PRERELEASE:
        source_toolchains_repo(
            name = "%s_toolchains" % name,
            toolchain_type = "@aspect_bazel_lib//lib:copy_to_directory_toolchain_type",
            toolchain_rule_load_from = "@aspect_bazel_lib//lib/private:copy_to_directory_toolchain.bzl",
            toolchain_rule = "copy_to_directory_toolchain",
            binary = "@aspect_bazel_lib//tools/copy_to_directory",
        )
        if register:
            native.register_toolchains("@%s_toolchains//:toolchain" % name)
        return

    for [platform, _] in COPY_TO_DIRECTORY_PLATFORMS.items():
        copy_to_directory_platform_repo(
            name = "%s_%s" % (name, platform),
            platform = platform,
        )
        if register:
            native.register_toolchains("@%s_toolchains//:%s_toolchain" % (name, platform))

    copy_to_directory_toolchains_repo(
        name = "%s_toolchains" % name,
        user_repository_name = name,
    )

DEFAULT_EXPAND_TEMPLATE_REPOSITORY = "expand_template"

def register_expand_template_toolchains(name = DEFAULT_EXPAND_TEMPLATE_REPOSITORY, register = True):
    """Registers expand_template toolchain and repositories

    Args:
        name: override the prefix for the generated toolchain repositories
        register: whether to call through to native.register_toolchains.
            Should be True for WORKSPACE users, but false when used under bzlmod extension
    """
    if IS_PRERELEASE:
        source_toolchains_repo(
            name = "%s_toolchains" % name,
            toolchain_type = "@aspect_bazel_lib//lib:expand_template_toolchain_type",
            toolchain_rule_load_from = "@aspect_bazel_lib//lib/private:expand_template_toolchain.bzl",
            toolchain_rule = "expand_template_toolchain",
            binary = "@aspect_bazel_lib//tools/expand_template",
        )
        if register:
            native.register_toolchains("@%s_toolchains//:toolchain" % name)
        return

    for [platform, _] in EXPAND_TEMPLATE_PLATFORMS.items():
        expand_template_platform_repo(
            name = "%s_%s" % (name, platform),
            platform = platform,
        )
        if register:
            native.register_toolchains("@%s_toolchains//:%s_toolchain" % (name, platform))

    expand_template_toolchains_repo(
        name = "%s_toolchains" % name,
        user_repository_name = name,
    )

# buildifier: disable=unnamed-macro
def aspect_bazel_lib_register_toolchains():
    """Register all bazel-lib toolchains at their default versions.

    To be more selective about which toolchains and versions to register,
    call the individual toolchain registration macros.
    """
    register_copy_directory_toolchains()
    register_copy_to_directory_toolchains()
    register_expand_template_toolchains()
    register_coreutils_toolchains()
    register_jq_toolchains()
    register_yq_toolchains()
    register_tar_toolchains()
    register_zstd_toolchains()
    register_bats_toolchains()
