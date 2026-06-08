"""Declares provider `PackageAttributeInfo`."""

visibility("public")

def _init(kind, attributes, files = []):
    return {
        "attributes": attributes,
        "files": depset(
            direct = [
                attributes,
            ],
            transitive = files,
        ),
        "kind": kind,
    }

PackageAttributeInfo, _create = provider(
    doc = """
Provider for declaring metadata about a Bazel package.

> **Fields in this provider are not covered by the stability gurantee.**
""".strip(),
    fields = {
        "attributes": """
The [File](https://bazel.build/rules/lib/builtins/File) containing the
attributes.

The format of this file depends on the `kind` of attribute. Please consult the
documentation of the attribute.
""".strip(),
        "files": """
A [depset](https://bazel.build/rules/lib/builtins/depset) of
[File](https://bazel.build/rules/lib/builtins/File)s containing information
about this attribute.
""".strip(),
        "kind": """
The identifier of the attribute.

This should generally be in reverse DNS format (e.g., `com.example.foo`).
""".strip(),
    },
    init = _init,
)
