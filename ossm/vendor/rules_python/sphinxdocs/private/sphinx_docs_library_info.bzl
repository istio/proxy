"""Provider for collecting doc files as libraries."""

SphinxDocsLibraryInfo = provider(
    doc = "Information about a collection of doc files.",
    fields = {
        "files": """
:type: depset[File]

The documentation files for the library.
""",
        "prefix": """
:type: str

Prefix to prepend to file paths in `files`. It is added after `strip_prefix`
is removed.
""",
        "strip_prefix": """
:type: str

Prefix to remove from file paths in `files`. It is removed before `prefix`
is prepended.
""",
        "transitive": """
:type: depset[struct]

Depset of transitive library information. Each entry in the depset is a struct
with fields matching the fields of this provider.
""",
    },
)
