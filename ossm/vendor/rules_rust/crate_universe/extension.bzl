"""Deprecated, use `:extensions.bzl`."""

load(
    ":extensions.bzl",
    _crate = "crate",
)

crate = _crate
