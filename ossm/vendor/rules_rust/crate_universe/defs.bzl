"""Crate Universe rules"""

load(
    "//crate_universe/private:crate.bzl",
    _crate = "crate",
)
load(
    "//crate_universe/private:crates_repository.bzl",
    _crates_repository = "crates_repository",
)
load(
    "//crate_universe/private:crates_vendor.bzl",
    _crates_vendor = "crates_vendor",
)
load(
    "//crate_universe/private:generate_utils.bzl",
    _render_config = "render_config",
)
load(
    "//crate_universe/private:splicing_utils.bzl",
    _splicing_config = "splicing_config",
)

# Rules
crates_repository = _crates_repository
crates_vendor = _crates_vendor

# Utility Macros
crate = _crate
render_config = _render_config
splicing_config = _splicing_config
