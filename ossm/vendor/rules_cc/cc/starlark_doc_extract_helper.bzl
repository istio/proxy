"""Indirection to avoid breaking on Bazel 6"""

visibility("private")

def starlark_doc_extract_helper(**kwargs):
    """Creates a starlark_doc_extract target if the native rule is available

    Args:
        **kwargs: (dict) args for starlark_doc_extract
    """
    if hasattr(native, "starlark_doc_extract"):
        native.starlark_doc_extract(**kwargs)
