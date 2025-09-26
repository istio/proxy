"""This is an experimental implementation of cc_shared_library.

We may change the implementation at any moment or even delete this file. Do not
rely on this. It requires bazel >1.2  and passing the flag
--experimental_cc_shared_library
"""

# Add this as a tag to any target that can be linked by more than one
# cc_shared_library because it doesn't have static initializers or anything
# else that may cause issues when being linked more than once. This should be
# used sparingly after making sure it's safe to use.
LINKABLE_MORE_THAN_ONCE = "LINKABLE_MORE_THAN_ONCE"

CcSharedLibraryPermissionsInfo = provider(
    "Permissions for a cc shared library.",
    fields = {
        "targets": "Matches targets that can be exported.",
    },
)
GraphNodeInfo = provider(
    "Nodes in the graph of shared libraries.",
    fields = {
        "children": "Other GraphNodeInfo from dependencies of this target",
        "label": "Label of the target visited",
        "linkable_more_than_once": "Linkable into more than a single cc_shared_library",
    },
)
CcSharedLibraryInfo = provider(
    "Information about a cc shared library.",
    fields = {
        "dynamic_deps": "All shared libraries depended on transitively",
        "exports": "cc_libraries that are linked statically and exported",
        "link_once_static_libs": "All libraries linked statically into this library that should " +
                                 "only be linked once, e.g. because they have static " +
                                 "initializers. If we try to link them more than once, " +
                                 "we will throw an error",
        "linker_input": "the resulting linker input artifact for the shared library",
        "preloaded_deps": "cc_libraries needed by this cc_shared_library that should" +
                          " be linked the binary. If this is set, this cc_shared_library has to " +
                          " be a direct dependency of the cc_binary",
    },
)

def cc_shared_library_permissions(**kwargs):
    native.cc_shared_library_permissions(**kwargs)

def cc_shared_library(**kwargs):
    native.cc_shared_library(**kwargs)  # buildifier: disable=native-cc-shared-library
