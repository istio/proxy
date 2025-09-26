# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project info

project = "Sphinx Stardoc Test"

extensions = [
    "sphinx_bzl.bzl",
    "myst_parser",
    "sphinx.ext.intersphinx",
]

myst_enable_extensions = [
    "fieldlist",
    "attrs_block",
    "attrs_inline",
    "colon_fence",
    "deflist",
    "substitution",
]

# --- Stardoc configuration

bzl_default_repository_name = "@testrepo"

# --- Intersphinx configuration

intersphinx_mapping = {
    "bazel": ("https://bazel.build/", "bazel_inventory.inv"),
}
