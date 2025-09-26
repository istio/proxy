"""Implementation of sphinx_docs_library macro."""

load("//python/private:util.bzl", "add_tag")  # buildifier: disable=bzl-visibility
load(":sphinx_docs_library.bzl", _sphinx_docs_library = "sphinx_docs_library")

def sphinx_docs_library(**kwargs):
    """Collection of doc files for use by `sphinx_docs`.

    Args:
        **kwargs: Args passed onto underlying {bzl:rule}`sphinx_docs_library` rule
    """
    add_tag(kwargs, "@rules_python//sphinxdocs:sphinx_docs_library")
    _sphinx_docs_library(**kwargs)
