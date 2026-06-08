def starlark_doc_extract__maybe(name, **kwargs):
    if hasattr(native, "starlark_doc_extract"):
        native.starlark_doc_extract(name = name, **kwargs)
