"""A rule that provides file(s) specific via DefaultInfo from a given target's DefaultInfo or OutputGroupInfo.

See also [select_file](https://github.com/bazelbuild/bazel-skylib/blob/main/docs/select_file_doc.md) from bazel-skylib.
"""

load(
    "//lib/private:output_files.bzl",
    _make_output_files = "make_output_files",
    _output_files = "output_files",
)

output_files = _output_files
make_output_files = _make_output_files
