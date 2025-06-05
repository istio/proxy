"Rules to be called from the users WORKSPACE"

load("//nodejs/private:os_name.bzl", "assert_node_exists_for_host", "node_exists_for_os")
load("//nodejs/private:node_versions.bzl", "NODE_VERSIONS")
load("//nodejs/private:nodejs_repo_host_os_alias.bzl", "nodejs_repo_host_os_alias")
load("//nodejs/private:toolchains_repo.bzl", "PLATFORMS", "toolchains_repo")
load("@bazel_skylib//lib:paths.bzl", "paths")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# Currently v16 is the default.
# We can only change that in a major release of rules_nodejs,
# as it's a semver-breaking change for our users who rely on it.
# We use the most recent v16 release.
DEFAULT_NODE_VERSION = [
    # 16.18.1-windows_amd64 -> 16.18.1
    v.split("-")[0]
    for v in NODE_VERSIONS.keys()
    if v.startswith("16.")
][-1]  # Versions are sorted increasing, so last one is the latest version

BUILT_IN_NODE_PLATFORMS = PLATFORMS.keys()

_DOC = """To be run in user's WORKSPACE to install rules_nodejs dependencies.

This rule sets up node, npm, and npx. The versions of these tools can be specified in one of three ways

### Simplest Usage

Specify no explicit versions. This will download and use the latest NodeJS that was available when the
version of rules_nodejs you're using was released.
Note that you can skip calling `node_repositories` in your WORKSPACE file - if you later try to `yarn_install` or `npm_install`,
we'll automatically select this simple usage for you.

### Forced version(s)

You can select the version of NodeJS to download & use by specifying it when you call node_repositories,
using a value that matches a known version (see the default values)

### Using a custom version

You can pass in a custom list of NodeJS repositories and URLs for node_repositories to use.

#### Custom NodeJS versions

To specify custom NodeJS versions, use the `node_repositories` attribute

```python
node_repositories(
    node_repositories = {
        "10.10.0-darwin_amd64": ("node-v10.10.0-darwin-x64.tar.gz", "node-v10.10.0-darwin-x64", "00b7a8426e076e9bf9d12ba2d571312e833fe962c70afafd10ad3682fdeeaa5e"),
        "10.10.0-linux_amd64": ("node-v10.10.0-linux-x64.tar.xz", "node-v10.10.0-linux-x64", "686d2c7b7698097e67bcd68edc3d6b5d28d81f62436c7cf9e7779d134ec262a9"),
        "10.10.0-windows_amd64": ("node-v10.10.0-win-x64.zip", "node-v10.10.0-win-x64", "70c46e6451798be9d052b700ce5dadccb75cf917f6bf0d6ed54344c856830cfb"),
    },
)
```

These can be mapped to a custom download URL, using `node_urls`

```python
node_repositories(
    node_version = "10.10.0",
    node_repositories = {"10.10.0-darwin_amd64": ("node-v10.10.0-darwin-x64.tar.gz", "node-v10.10.0-darwin-x64", "00b7a8426e076e9bf9d12ba2d571312e833fe962c70afafd10ad3682fdeeaa5e")},
    node_urls = ["https://mycorpproxy/mirror/node/v{version}/{filename}"],
)
```

A Mac client will try to download node from `https://mycorpproxy/mirror/node/v10.10.0/node-v10.10.0-darwin-x64.tar.gz`
and expect that file to have sha256sum `00b7a8426e076e9bf9d12ba2d571312e833fe962c70afafd10ad3682fdeeaa5e`

See the [the repositories documentation](repositories.html) for how to use the resulting repositories.

### Using a custom node.js.

To avoid downloads, you can check in a vendored node.js binary or can build one from source.
See [toolchains](./toolchains.md) and `examples/vendored_node_and_yarn`.

"""

_ATTRS = {
    "node_download_auth": attr.string_dict(
        default = {},
        doc = """auth to use for all url requests
Example: {\"type\": \"basic\", \"login\": \"<UserName>\", \"password\": \"<Password>\" }
""",
    ),
    "node_repositories": attr.string_list_dict(
        doc = """Custom list of node repositories to use

A dictionary mapping NodeJS versions to sets of hosts and their corresponding (filename, strip_prefix, sha256) tuples.
You should list a node binary for every platform users have, likely Mac, Windows, and Linux.

By default, if this attribute has no items, we'll use a list of all public NodeJS releases.
""",
    ),
    "node_urls": attr.string_list(
        default = [
            "https://nodejs.org/dist/v{version}/{filename}",
        ],
        doc = """custom list of URLs to use to download NodeJS

Each entry is a template for downloading a node distribution.

The `{version}` parameter is substituted with the `node_version` attribute,
and `{filename}` with the matching entry from the `node_repositories` attribute.
""",
    ),
    "node_version": attr.string(
        default = DEFAULT_NODE_VERSION,
        doc = "the specific version of NodeJS to install",
    ),
    "use_nvmrc": attr.label(
        allow_single_file = True,
        default = None,
        doc = """the local path of the .nvmrc file containing the version of node

If set then also set node_version to the version found in the .nvmrc file.""",
    ),
    "platform": attr.string(
        doc = "Internal use only. Which platform to install as a toolchain. If unset, we assume the repository is named nodejs_[platform]",
        values = BUILT_IN_NODE_PLATFORMS,
    ),
}

NODE_EXTRACT_DIR = "bin/nodejs"

GET_SCRIPT_DIR = """
# From stackoverflow.com
SOURCE="${BASH_SOURCE[0]}"
# Resolve $SOURCE until the file is no longer a symlink
while [ -h "$SOURCE" ]; do
  DIR="$(cd -P "$(dirname "$SOURCE" )" >/dev/null && pwd)"
  SOURCE="$(readlink "$SOURCE")"
  # if $SOURCE was a relative symlink, we need to resolve it relative to the
  # path where the symlink file was located.
  [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE"
done
SCRIPT_DIR="$(cd -P "$( dirname "$SOURCE" )" >/dev/null && pwd)"
"""

def _download_node(repository_ctx):
    """Used to download a NodeJS runtime package.

    Args:
      repository_ctx: The repository rule context
    """

    # If platform is unset, we assume the repository follows the naming convention
    # @nodejs_PLATFORM where PLATFORM is one of BUILT_IN_NODE_PLATFORMS
    host_os = repository_ctx.attr.platform or repository_ctx.name.split("nodejs_", 1)[1]

    node_version = repository_ctx.attr.node_version

    if repository_ctx.attr.use_nvmrc:
        node_version = str(repository_ctx.read(repository_ctx.attr.use_nvmrc)).strip()

    _verify_version_is_valid(node_version)

    node_repositories = repository_ctx.attr.node_repositories

    # We insert our default value here, not on the attribute's default, so it isn't documented.
    # The size of NODE_VERSIONS constant is huge and not useful to document.
    if not node_repositories.items():
        node_repositories = NODE_VERSIONS

    # Skip the download if we know it will fail
    if not node_exists_for_os(node_version, host_os, node_repositories):
        return

    node_urls = repository_ctx.attr.node_urls

    # Download node & npm
    version_host_os = "%s-%s" % (node_version, host_os)
    if not version_host_os in node_repositories:
        fail("Unknown NodeJS version-host %s" % version_host_os)
    filename, strip_prefix, sha256 = node_repositories[version_host_os]

    urls = [url.format(version = node_version, filename = filename) for url in node_urls]
    auth = {}
    for url in urls:
        auth[url] = repository_ctx.attr.node_download_auth

    repository_ctx.download_and_extract(
        auth = auth,
        url = urls,
        output = NODE_EXTRACT_DIR,
        stripPrefix = strip_prefix,
        sha256 = sha256,
    )

    repository_ctx.file("node_info", content = """# filename: {filename}
# strip_prefix: {strip_prefix}
# sha256: {sha256}
""".format(
        filename = filename,
        strip_prefix = strip_prefix,
        sha256 = sha256,
    ))

def _prepare_node(repository_ctx):
    """Sets up BUILD files and shell wrappers for the versions of NodeJS, npm & yarn just set up.

    Windows and other OSes set up the node runtime with different names and paths, which we hide away via
    the BUILD file here.
    In addition, we create a bash script wrapper around NPM that passes a given NPM command to all package.json labels
    passed into here.
    Finally, we create a reusable template bash script around NPM that is used by rules like pkg_npm to access
    NPM.

    Args:
      repository_ctx: The repository rule context
    """

    # TODO: Maybe we want to encode the OS as a specific attribute rather than do it based on naming?
    is_windows = "_windows_" in repository_ctx.attr.name

    node_path = NODE_EXTRACT_DIR
    node_package = NODE_EXTRACT_DIR
    node_bin = ("%s/bin/node" % node_path) if not is_windows else ("%s/node.exe" % node_path)
    node_bin_label = ("%s/bin/node" % node_package) if not is_windows else ("%s/node.exe" % node_package)

    # Use the npm-cli.js script as the bin for osx & linux so there are no symlink issues with `%s/bin/npm`
    npm_bin = ("%s/lib/node_modules/npm/bin/npm-cli.js" % node_path) if not is_windows else ("%s/npm.cmd" % node_path)
    npm_bin_label = ("%s/lib/node_modules/npm/bin/npm-cli.js" % node_package) if not is_windows else ("%s/npm.cmd" % node_package)
    npm_script = ("%s/lib/node_modules/npm/bin/npm-cli.js" % node_path) if not is_windows else ("%s/node_modules/npm/bin/npm-cli.js" % node_path)

    # Use the npx-cli.js script as the bin for osx & linux so there are no symlink issues with `%s/bin/npx`
    npx_bin = ("%s/lib/node_modules/npm/bin/npx-cli.js" % node_path) if not is_windows else ("%s/npx.cmd" % node_path)
    npx_bin_label = ("%s/lib/node_modules/npm/bin/npx-cli.js" % node_package) if not is_windows else ("%s/npx.cmd" % node_package)

    entry_ext = ".cmd" if is_windows else ""
    node_entry = "bin/node%s" % entry_ext
    npm_entry = "bin/npm%s" % entry_ext

    node_bin_relative = paths.relativize(node_bin, "bin")
    npm_script_relative = paths.relativize(npm_script, "bin")

    # The entry points for node for osx/linux and windows
    if not is_windows:
        # Sets PATH and runs the application
        repository_ctx.file("bin/node", content = """#!/usr/bin/env bash
# Generated by node_repositories.bzl
# Immediately exit if any command fails.
set -e
{get_script_dir}
export PATH="$SCRIPT_DIR":$PATH
exec "$SCRIPT_DIR/{node}" "$@"
""".format(
            get_script_dir = GET_SCRIPT_DIR,
            node = node_bin_relative,
        ))
    else:
        # Sets PATH for node, npm & yarn and run user script
        repository_ctx.file("bin/node.cmd", content = """
@echo off
SET SCRIPT_DIR=%~dp0
SET PATH=%SCRIPT_DIR%;%PATH%
CALL "%SCRIPT_DIR%\\{node}" %*
""".format(node = node_bin_relative))

    # The entry points for npm for osx/linux and windows
    # Runs npm using appropriate node entry point
    # --scripts-prepend-node-path is set to false since the correct paths
    # for the Bazel entry points of node, npm & yarn are set in the node
    # entry point
    if not is_windows:
        # Npm entry point
        repository_ctx.file(
            "bin/npm",
            content = """#!/usr/bin/env bash
# Generated by node_repositories.bzl
# Immediately exit if any command fails.
set -e
{get_script_dir}
"$SCRIPT_DIR/{node}" "$SCRIPT_DIR/{script}" --scripts-prepend-node-path=false "$@"
""".format(
                get_script_dir = GET_SCRIPT_DIR,
                node = paths.relativize(node_entry, "bin"),
                script = npm_script_relative,
            ),
            executable = True,
        )
    else:
        # Npm entry point
        repository_ctx.file(
            "bin/npm.cmd",
            content = """@echo off
SET SCRIPT_DIR=%~dp0
"%SCRIPT_DIR%\\{node}" "%SCRIPT_DIR%\\{script}" --scripts-prepend-node-path=false %*
""".format(
                node = paths.relativize(node_entry, "bin"),
                script = npm_script_relative,
            ),
            executable = True,
        )

    # This template file is used by the packager tool and the pkg_npm rule.
    # `yarn publish` is not ready for use under Bazel, see https://github.com/yarnpkg/yarn/issues/610
    if repository_ctx.attr.platform.startswith("windows"):
        run_npm = """
"{node}" "{script}" TMPL_args %*
"""
    else:
        run_npm = """
"{node}" "{script}" TMPL_args "$@"
"""

    repository_ctx.file("run_npm.template", content = run_npm.format(
        node = repository_ctx.path(node_entry),
        script = repository_ctx.path(npm_script),
    ))

    # Base BUILD file for this repository
    build_content = """# Generated by node_repositories.bzl
package(default_visibility = ["//visibility:public"])
exports_files([
  "run_npm.template",
  "{node_entry}",
  "{npm_entry}",
  ])
alias(name = "node_bin", actual = "{node_bin_label}")
alias(name = "npm_bin", actual = "{npm_bin_label}")
alias(name = "npx_bin", actual = "{npx_bin_label}")
alias(name = "node", actual = "{node_entry}")
alias(name = "npm", actual = "{npm_entry}")
filegroup(
  name = "node_files",
  srcs = [":node", ":node_bin"],
)
filegroup(
  name = "npm_files",
  srcs = {npm_files_glob}[":node_files"],
)
""".format(
        node_bin_export = "\n  \"%s\"," % node_bin,
        npm_bin_export = "\n  \"%s\"," % npm_bin,
        npx_bin_export = "\n  \"%s\"," % npx_bin,
        npm_files_glob = "glob([\"bin/nodejs/**\"]) + ",
        node_bin_label = node_bin_label,
        npm_bin_label = npm_bin_label,
        npx_bin_label = npx_bin_label,
        node_entry = node_entry,
        npm_entry = npm_entry,
    )

    # the platform attribute is only set when used from this file, not from build_bazel_rules_nodejs
    if repository_ctx.attr.platform:
        build_content += """
load("@rules_nodejs//nodejs:toolchain.bzl", "node_toolchain")
node_toolchain(
    name = "node_toolchain",
    target_tool = ":node_bin",
    npm = ":npm",
    npm_files = [":npm_files"],
    run_npm = ":run_npm.template",
)
"""
    repository_ctx.file("BUILD.bazel", content = build_content)

def _verify_version_is_valid(version):
    major, minor, patch = (version.split(".") + [None, None, None])[:3]
    if not major.isdigit() or not minor.isdigit() or not patch.isdigit():
        fail("Invalid node version: %s" % version)

def _nodejs_repo_impl(repository_ctx):
    assert_node_exists_for_host(repository_ctx)
    _download_node(repository_ctx)
    _prepare_node(repository_ctx)

node_repositories = repository_rule(
    _nodejs_repo_impl,
    doc = _DOC,
    attrs = _ATTRS,
)

# Wrapper macro around everything above, this is the primary API
def nodejs_register_toolchains(name, register = True, **kwargs):
    """Convenience macro for users which does typical setup.

    - create a repository for each built-in platform like "node16_linux_amd64" -
      this repository is lazily fetched when node is needed for that platform.
    - create a convenience repository for the host platform like "node16_host"
    - create a repository exposing toolchains for each platform like "node16_platforms"
    - register a toolchain pointing at each platform

    Users can avoid this macro and do these steps themselves, if they want more control.

    Args:
        name: base name for all created repos, like "node16"
        register: whether to call Bazel register_toolchains on the created toolchains.
            Should be True when used from a WORKSPACE file, and False used from bzlmod
            which has its own toolchain registration syntax.
        **kwargs: passed to each node_repositories call
    """
    for platform in BUILT_IN_NODE_PLATFORMS:
        node_repositories(
            name = name + "_" + platform,
            platform = platform,
            **kwargs
        )
        if register:
            native.register_toolchains(
                "@%s_toolchains//:%s_toolchain_target" % (name, platform),
                "@%s_toolchains//:%s_toolchain" % (name, platform),
            )

    nodejs_repo_host_os_alias(
        name = name,
        user_node_repository_name = name,
    )

    # For backwards compatibility, also provide it under the name with _host suffix.
    nodejs_repo_host_os_alias(
        name = name + "_host",
        user_node_repository_name = name,
    )
    toolchains_repo(
        name = name + "_toolchains",
        user_node_repository_name = name,
    )

def rules_nodejs_dependencies():
    maybe(
        http_archive,
        name = "bazel_skylib",
        sha256 = "c6966ec828da198c5d9adbaa94c05e3a1c7f21bd012a0b29ba8ddbccb2c93b0d",
        urls = [
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.1.1/bazel-skylib-1.1.1.tar.gz",
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.1.1/bazel-skylib-1.1.1.tar.gz",
        ],
    )
