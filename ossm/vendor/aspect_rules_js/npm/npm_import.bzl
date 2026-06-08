"""DEPRECATED: see //npm:repositories.bzl.
"""

load(":repositories.bzl", _LATEST_PNPM_VERSION = "LATEST_PNPM_VERSION", _list_patches = "list_patches", _npm_import = "npm_import", _npm_translate_lock = "npm_translate_lock", _pnpm_repository = "pnpm_repository")

# TODO(2.0): remove deprecated re-exports
npm_translate_lock = _npm_translate_lock
pnpm_repository = _pnpm_repository
LATEST_PNPM_VERSION = _LATEST_PNPM_VERSION
list_patches = _list_patches
npm_import = _npm_import
