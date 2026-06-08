"""Rule and corresponding provider that joins a label pointing to a TreeArtifact
with a path nested within that directory
"""

load(
    "//lib/private:directory_path.bzl",
    _DirectoryPathInfo = "DirectoryPathInfo",
    _directory_path = "directory_path",
    _make_directory_path = "make_directory_path",
    _make_directory_paths = "make_directory_paths",
)

directory_path = _directory_path
make_directory_path = _make_directory_path
make_directory_paths = _make_directory_paths
DirectoryPathInfo = _DirectoryPathInfo
