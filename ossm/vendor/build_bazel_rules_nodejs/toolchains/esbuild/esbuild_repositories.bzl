"""
Helper macro for fetching esbuild versions
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@build_bazel_rules_nodejs//:index.bzl", "npm_install")
load(":esbuild_packages.bzl", "ESBUILD_PACKAGES")

def esbuild_repositories(npm_repository, name = "", npm_args = [], **kwargs):
    """Helper for fetching and setting up the esbuild versions and toolchains

    This uses Bazel's downloader (via `http_archive`) to fetch the esbuild package
    from npm, separately from any `npm_install`/`yarn_install` in your WORKSPACE.
    To configure where the download is from, you make a file containing a rewrite rule like

        rewrite (registry.nodejs.org)/(.*) artifactory.build.internal.net/artifactory/$1/$2

    You can find some documentation on the rewrite patterns in the Bazel sources:
    [UrlRewriterConfig.java](https://github.com/bazelbuild/bazel/blob/4.2.1/src/main/java/com/google/devtools/build/lib/bazel/repository/downloader/UrlRewriterConfig.java#L66)

    Then use the `--experimental_downloader_config` Bazel option to point to your file.
    For example if you created `.bazel_downloader_config` you might add to your `.bazelrc` file:

        common --experimental_downloader_config=.bazel_downloader_config

    Args:
        npm_repository:  the name of the repository where the @bazel/esbuild package is installed
            by npm_install or yarn_install.
        name: currently unused
        npm_args: additional args to pass to the npm install rule
        **kwargs: additional named parameters to the npm_install rule
    """

    maybe(
        http_archive,
        name = "bazel_skylib",
        sha256 = "c6966ec828da198c5d9adbaa94c05e3a1c7f21bd012a0b29ba8ddbccb2c93b0d",
        urls = [
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.1.1/bazel-skylib-1.1.1.tar.gz",
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.1.1/bazel-skylib-1.1.1.tar.gz",
        ],
    )

    for name, meta in ESBUILD_PACKAGES.platforms.items():
        maybe(
            http_archive,
            name = "esbuild_%s" % name,
            urls = meta.urls,
            strip_prefix = "package",
            build_file_content = """exports_files(["%s"])""" % meta.binary_path,
            sha256 = meta.sha,
        )

        toolchain_label = Label("@build_bazel_rules_nodejs//toolchains/esbuild:esbuild_%s_toolchain" % name)
        native.register_toolchains("@%s//%s:%s" % (toolchain_label.workspace_name, toolchain_label.package, toolchain_label.name))

    # When used from our distribution, the toolchain in rules_nodejs needs to point out to the
    # @bazel/esbuild package where it was installed by npm_install so that our launcher.js can
    # require('esbuild') via the multi-linker.
    pkg_label = Label("@%s//packages/esbuild:esbuild.bzl" % npm_repository)
    package_path = "external/" + pkg_label.workspace_name + "/@bazel/esbuild"

    
    npm_install(
        name = "esbuild_npm",
        package_json = Label("@build_bazel_rules_nodejs//toolchains/esbuild:package.json"),
        package_lock_json = Label("@build_bazel_rules_nodejs//toolchains/esbuild:package-lock.json"),
        args = [
            # Install is run with no-optional so that esbuild's optional dependencies are not installed.
            # We never use the downloaded binary anyway and instead set 'ESBUILD_BINARY_PATH' to the toolchains path.
            # This allows us to deal with --platform
            "--no-optional",
            # Disable scripts as we don't need the javascript shim replaced wit the binary.
            "--ignore-scripts",
        ] + npm_args,
        package_path = package_path,
        **kwargs
    )
