"""# rules_nodejs Bazel module

This is the "core" module, and is used internally by `build_bazel_rules_nodejs`.
Most users should continue to use only the latter, and ignore this "core" module.

The dependency graph is:
`build_bazel_rules_nodejs -> rules_nodejs -> bazel_skylib`

Features:
- A [Toolchain](https://docs.bazel.build/versions/main/toolchains.html) 
  that fetches a hermetic copy of node, npm, and yarn - independent of what's on the developer's machine.
- Core [Providers](https://docs.bazel.build/versions/main/skylark/rules.html#providers) to allow interop between JS rules.

Most features, such as `npm_install` and `nodejs_binary` are still in the `build_bazel_rules_nodejs` module.
We plan to clean these up and port into `rules_nodejs` in a future major release.
"""

load(
    ":providers.bzl",
    _DeclarationInfo = "DeclarationInfo",
    _DirectoryFilePathInfo = "DirectoryFilePathInfo",
    _JSModuleInfo = "JSModuleInfo",
    _LinkablePackageInfo = "LinkablePackageInfo",
    _UserBuildSettingInfo = "UserBuildSettingInfo",
    _declaration_info = "declaration_info",
    _js_module_info = "js_module_info",
)
load(":directory_file_path.bzl", _directory_file_path = "directory_file_path")
load(":repositories.bzl", _node_repositories = "node_repositories")
load(":yarn_repositories.bzl", _yarn_repositories = "yarn_repositories")
load(":toolchain.bzl", _node_toolchain = "node_toolchain")

DeclarationInfo = _DeclarationInfo
declaration_info = _declaration_info
directory_file_path = _directory_file_path
JSModuleInfo = _JSModuleInfo
js_module_info = _js_module_info
LinkablePackageInfo = _LinkablePackageInfo
DirectoryFilePathInfo = _DirectoryFilePathInfo
UserBuildSettingInfo = _UserBuildSettingInfo
node_repositories = _node_repositories
node_toolchain = _node_toolchain
yarn_repositories = _yarn_repositories
