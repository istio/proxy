# Copyright 2020 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Packaging related providers."""

PackageArtifactInfo = provider(
    doc = """Metadata about a package artifact.""",
    fields = {
        "file": "File object for said artifact",
        "file_name": "DEPRECATED (use fields of file instead): The file name of the artifact.",
        "label": "Label which produced it",
    },
)

PackageVariablesInfo = provider(
    doc = """Variables which may be substituted into package names and content.""",
    fields = {
        "values": "Dict of name/value pairs",
    },
)

PackageFilesInfo = provider(
    doc = """Provider representing the installation of one or more files to destination with attributes""",
    fields = {
        "attributes": """Attribute information, represented as a `dict`.

Keys are strings representing attribute identifiers, values are
arbitrary data structures that represent the associated data.  These are
most often strings, but are not explicitly defined.

For known attributes and data type expectations, see the Common
Attributes documentation in the `rules_pkg` reference.
        """,

        # This is a mapping of destinations to sources to allow for the same
        # target to be installed to multiple locations within a package within a
        # single provider.
        "dest_src_map": """Map of file destinations to sources.

        Sources are represented by bazel `File` structures.""",
    },
)

PackageDirsInfo = provider(
    doc = """Provider representing the creation of one or more directories in a package""",
    fields = {
        "attributes": """See `attributes` in PackageFilesInfo.""",
        "dirs": """string list: installed directory names""",
    },
)

PackageSymlinkInfo = provider(
    doc = """Provider representing the creation of a single symbolic link in a package""",
    fields = {
        "attributes": """See `attributes` in PackageFilesInfo.""",
        "destination": """string: Filesystem link 'name'""",
        "target": """string or Label: Filesystem link 'target'.

        TODO(nacl): Label sources not yet supported.
        """,
    },
)

# Grouping provider: the only one that needs to be consumed by packaging (or
# other) rules that materialize paths.
PackageFilegroupInfo = provider(
    doc = """Provider representing a collection of related packaging providers,

    In the "fields" documentation, "origin" refers to the label identifying the
    where the provider was originally defined.  This can be used by packaging
    rules to provide better diagnostics related to where packaging rules were
    created.

    """,
    fields = {
        "pkg_files": "list of tuples of (PackageFilesInfo, origin)",
        "pkg_dirs": "list of tuples of (PackageDirsInfo, origin)",
        "pkg_symlinks": "list of tuples of (PackageSymlinkInfo, origin)",
    },
)
