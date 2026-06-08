"""Repository rules to fetch third-party npm packages"""

load("@bazel_features//:deps.bzl", "bazel_features_deps")
load("//npm/private:npm_import.bzl", _npm_import = "npm_import")
load("//npm/private:npm_translate_lock.bzl", _list_patches = "list_patches", _npm_translate_lock = "npm_translate_lock")
load("//npm/private:pnpm_repository.bzl", _LATEST_PNPM_VERSION = "LATEST_PNPM_VERSION", _pnpm_repository = "pnpm_repository")

npm_import = _npm_import

def npm_translate_lock(**kwargs):
    bazel_features_deps()
    _npm_translate_lock(**kwargs)

pnpm_repository = _pnpm_repository
LATEST_PNPM_VERSION = _LATEST_PNPM_VERSION
list_patches = _list_patches
