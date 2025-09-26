"""
Transitive dependencies.
"""

load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

def cel_spec_deps_extra():
    """CEL Spec dependencies."""
    protobuf_deps()

def cel_cpp_deps_extra():
    """All transitive dependencies."""
    switched_rules_by_language(
        name = "com_google_googleapis_imports",
        cc = True,
    )
    cel_spec_deps_extra()
