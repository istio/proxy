# Copyright 2018 The Bazel Authors. All rights reserved.
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

from __future__ import annotations

import argparse
import base64
import csv
import hashlib
import io
import os
import re
import stat
import sys
import zipfile
from pathlib import Path

_ZIP_EPOCH = (1980, 1, 1, 0, 0, 0)


def commonpath(path1, path2):
    ret = []
    for a, b in zip(path1.split(os.path.sep), path2.split(os.path.sep)):
        if a != b:
            break
        ret.append(a)
    return os.path.sep.join(ret)


def escape_filename_segment(segment):
    """Escapes a filename segment per https://www.python.org/dev/peps/pep-0427/#escaping-and-unicode

    This is a legacy function, kept for backwards compatibility,
    and may be removed in the future. See `escape_filename_distribution_name`
    and `normalize_pep440` for the modern alternatives.
    """
    return re.sub(r"[^\w\d.]+", "_", segment, re.UNICODE)


def normalize_package_name(name):
    """Normalize a package name according to the Python Packaging User Guide.

    See https://packaging.python.org/en/latest/specifications/name-normalization/
    """
    return re.sub(r"[-_.]+", "-", name).lower()


def escape_filename_distribution_name(name):
    """Escape the distribution name component of a filename.

    See https://packaging.python.org/en/latest/specifications/binary-distribution-format/#escaping-and-unicode
    """
    return normalize_package_name(name).replace("-", "_")


def normalize_pep440(version):
    """Normalize version according to PEP 440, with fallback for placeholders.

    If there's a placeholder in braces, such as {BUILD_TIMESTAMP},
    replace it with 0. Such placeholders can be used with stamping, in
    which case they would have been resolved already by now; if they
    haven't, we're doing an unstamped build, but we still need to
    produce a valid version. If such replacements are made, the
    original version string, sanitized to dot-separated alphanumerics,
    is appended as a local version segment, so you understand what
    placeholder was involved.

    If that still doesn't produce a valid version, use version 0 and
    append the original version string, sanitized to dot-separated
    alphanumerics, as a local version segment.

    """

    import packaging.version

    try:
        return str(packaging.version.Version(version))
    except packaging.version.InvalidVersion:
        pass

    sanitized = re.sub(r"[^a-z0-9]+", ".", version.lower()).strip(".")
    substituted = re.sub(r"\{\w+\}", "0", version)
    delimiter = "." if "+" in substituted else "+"
    try:
        return str(packaging.version.Version(f"{substituted}{delimiter}{sanitized}"))
    except packaging.version.InvalidVersion:
        return str(packaging.version.Version(f"0+{sanitized}"))


class _WhlFile(zipfile.ZipFile):
    def __init__(
        self,
        filename,
        *,
        mode,
        distribution_prefix: str,
        strip_path_prefixes=None,
        compression=zipfile.ZIP_DEFLATED,
        **kwargs,
    ):
        self._distribution_prefix = distribution_prefix

        self._strip_path_prefixes = strip_path_prefixes or []
        # Entries for the RECORD file as (filename, hash, size) tuples.
        self._record = []

        super().__init__(filename, mode=mode, compression=compression, **kwargs)

    def distinfo_path(self, basename):
        return f"{self._distribution_prefix}.dist-info/{basename}"

    def data_path(self, basename):
        return f"{self._distribution_prefix}.data/{basename}"

    def add_file(self, package_filename, real_filename):
        """Add given file to the distribution."""

        def arcname_from(name):
            # Always use unix path separators.
            normalized_arcname = name.replace(os.path.sep, "/")
            # Don't manipulate names filenames in the .distinfo or .data directories.
            if normalized_arcname.startswith(self._distribution_prefix):
                return normalized_arcname
            for prefix in self._strip_path_prefixes:
                if normalized_arcname.startswith(prefix):
                    return normalized_arcname[len(prefix) :]

            return normalized_arcname

        if os.path.isdir(real_filename):
            directory_contents = os.listdir(real_filename)
            for file_ in directory_contents:
                self.add_file(
                    "{}/{}".format(package_filename, file_),
                    "{}/{}".format(real_filename, file_),
                )
            return

        arcname = arcname_from(package_filename)
        zinfo = self._zipinfo(arcname)

        # Write file to the zip archive while computing the hash and length
        hash = hashlib.sha256()
        size = 0
        with open(real_filename, "rb") as fsrc:
            with self.open(zinfo, "w") as fdst:
                while True:
                    block = fsrc.read(2**20)
                    if not block:
                        break
                    fdst.write(block)
                    hash.update(block)
                    size += len(block)

        self._add_to_record(arcname, self._serialize_digest(hash), size)

    def add_string(self, filename, contents):
        """Add given 'contents' as filename to the distribution."""
        if isinstance(contents, str):
            contents = contents.encode("utf-8", "surrogateescape")
        zinfo = self._zipinfo(filename)
        self.writestr(zinfo, contents)
        hash = hashlib.sha256()
        hash.update(contents)
        self._add_to_record(filename, self._serialize_digest(hash), len(contents))

    def _serialize_digest(self, hash):
        # https://www.python.org/dev/peps/pep-0376/#record
        # "base64.urlsafe_b64encode(digest) with trailing = removed"
        digest = base64.urlsafe_b64encode(hash.digest())
        digest = b"sha256=" + digest.rstrip(b"=")
        return digest

    def _add_to_record(self, filename, hash, size):
        size = str(size).encode("ascii")
        self._record.append((filename, hash, size))

    def _zipinfo(self, filename):
        """Construct deterministic ZipInfo entry for a file named filename"""
        # Strip leading path separators to mirror ZipInfo.from_file behavior
        separators = os.path.sep
        if os.path.altsep is not None:
            separators += os.path.altsep
        arcname = filename.lstrip(separators)

        zinfo = zipfile.ZipInfo(filename=arcname, date_time=_ZIP_EPOCH)
        zinfo.create_system = 3  # ZipInfo entry created on a unix-y system
        # Both pip and installer expect the regular file bit to be set in order for the
        # executable bit to be preserved after extraction
        # https://github.com/pypa/pip/blob/23.3.2/src/pip/_internal/utils/unpacking.py#L96-L100
        # https://github.com/pypa/installer/blob/0.7.0/src/installer/sources.py#L310-L313
        zinfo.external_attr = (
            stat.S_IRWXU | stat.S_IRWXG | stat.S_IRWXO | stat.S_IFREG
        ) << 16  # permissions: -rwxrwxrwx
        zinfo.compress_type = self.compression
        return zinfo

    def add_recordfile(self):
        """Write RECORD file to the distribution."""
        record_path = self.distinfo_path("RECORD")
        entries = self._record + [(record_path, b"", b"")]
        with io.StringIO() as contents_io:
            writer = csv.writer(contents_io, lineterminator="\n")
            for filename, digest, size in entries:
                if isinstance(filename, str):
                    filename = filename.lstrip("/")
                writer.writerow(
                    (
                        c
                        if isinstance(c, str)
                        else c.decode("utf-8", "surrogateescape")
                        for c in (filename, digest, size)
                    )
                )

            contents = contents_io.getvalue()
            self.add_string(record_path, contents)
            return contents.encode("utf-8", "surrogateescape")


class WheelMaker(object):
    def __init__(
        self,
        name,
        version,
        build_tag,
        python_tag,
        abi,
        platform,
        compress,
        outfile=None,
        strip_path_prefixes=None,
    ):
        self._name = name
        self._version = normalize_pep440(version)
        self._build_tag = build_tag
        self._python_tag = python_tag
        self._abi = abi
        self._platform = platform
        self._outfile = outfile
        self._strip_path_prefixes = strip_path_prefixes
        self._compress = compress
        self._wheelname_fragment_distribution_name = escape_filename_distribution_name(
            self._name
        )

        self._distribution_prefix = (
            self._wheelname_fragment_distribution_name + "-" + self._version
        )

        self._whlfile = None

    def __enter__(self):
        self._whlfile = _WhlFile(
            self.filename(),
            mode="w",
            distribution_prefix=self._distribution_prefix,
            strip_path_prefixes=self._strip_path_prefixes,
            compression=zipfile.ZIP_DEFLATED if self._compress else zipfile.ZIP_STORED,
        )
        return self

    def __exit__(self, type, value, traceback):
        self._whlfile.close()
        self._whlfile = None

    def wheelname(self) -> str:
        components = [
            self._wheelname_fragment_distribution_name,
            self._version,
        ]
        if self._build_tag:
            components.append(self._build_tag)
        components += [self._python_tag, self._abi, self._platform]
        return "-".join(components) + ".whl"

    def filename(self) -> str:
        if self._outfile:
            return self._outfile
        return self.wheelname()

    def disttags(self):
        return ["-".join([self._python_tag, self._abi, self._platform])]

    def distinfo_path(self, basename):
        return self._whlfile.distinfo_path(basename)

    def data_path(self, basename):
        return self._whlfile.data_path(basename)

    def add_file(self, package_filename, real_filename):
        """Add given file to the distribution."""
        self._whlfile.add_file(package_filename, real_filename)

    def add_wheelfile(self):
        """Write WHEEL file to the distribution"""
        # TODO(pstradomski): Support non-purelib wheels.
        wheel_contents = """\
Wheel-Version: 1.0
Generator: bazel-wheelmaker 1.0
Root-Is-Purelib: {}
""".format(
            "true" if self._platform == "any" else "false"
        )
        for tag in self.disttags():
            wheel_contents += "Tag: %s\n" % tag
        self._whlfile.add_string(self.distinfo_path("WHEEL"), wheel_contents)

    def add_metadata(self, metadata, name, description):
        """Write METADATA file to the distribution."""
        # https://www.python.org/dev/peps/pep-0566/
        # https://packaging.python.org/specifications/core-metadata/
        metadata = re.sub("^Name: .*$", "Name: %s" % name, metadata, flags=re.MULTILINE)
        metadata += "Version: %s\n\n" % self._version
        # setuptools seems to insert UNKNOWN as description when none is
        # provided.
        metadata += description if description else "UNKNOWN"
        metadata += "\n"
        self._whlfile.add_string(self.distinfo_path("METADATA"), metadata)

    def add_recordfile(self):
        """Write RECORD file to the distribution."""
        self._whlfile.add_recordfile()


def get_files_to_package(input_files):
    """Find files to be added to the distribution.

    input_files: list of pairs (package_path, real_path)
    """
    files = {}
    for package_path, real_path in input_files:
        files[package_path] = real_path
    return files


def resolve_argument_stamp(
    argument: str, volatile_status_stamp: Path, stable_status_stamp: Path
) -> str:
    """Resolve workspace status stamps format strings found in the argument string

    Args:
        argument (str): The raw argument represenation for the wheel (may include stamp variables)
        volatile_status_stamp (Path): The path to a volatile workspace status file
        stable_status_stamp (Path): The path to a stable workspace status file

    Returns:
        str: A resolved argument string
    """
    lines = (
        volatile_status_stamp.read_text().splitlines()
        + stable_status_stamp.read_text().splitlines()
    )
    for line in lines:
        if not line:
            continue
        key, value = line.split(" ", maxsplit=1)
        stamp = "{" + key + "}"
        argument = argument.replace(stamp, value)

    return argument


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Builds a python wheel")
    metadata_group = parser.add_argument_group("Wheel name, version and platform")
    metadata_group.add_argument(
        "--name", required=True, type=str, help="Name of the distribution"
    )
    metadata_group.add_argument(
        "--version", required=True, type=str, help="Version of the distribution"
    )
    metadata_group.add_argument(
        "--build_tag",
        type=str,
        default="",
        help="Optional build tag for the distribution",
    )
    metadata_group.add_argument(
        "--python_tag",
        type=str,
        default="py3",
        help="Python version, e.g. 'py2' or 'py3'",
    )
    metadata_group.add_argument("--abi", type=str, default="none")
    metadata_group.add_argument(
        "--platform", type=str, default="any", help="Target platform. "
    )

    output_group = parser.add_argument_group("Output file location")
    output_group.add_argument(
        "--out", type=str, default=None, help="Override name of ouptut file"
    )
    output_group.add_argument(
        "--no_compress",
        action="store_true",
        help="Disable compression of the final archive",
    )
    output_group.add_argument(
        "--name_file",
        type=Path,
        help="A file where the canonical name of the " "wheel will be written",
    )

    output_group.add_argument(
        "--strip_path_prefix",
        type=str,
        action="append",
        default=[],
        help="Path prefix to be stripped from input package files' path. "
        "Can be supplied multiple times. Evaluated in order.",
    )

    wheel_group = parser.add_argument_group("Wheel metadata")
    wheel_group.add_argument(
        "--metadata_file",
        type=Path,
        help="Contents of the METADATA file (before appending contents of "
        "--description_file)",
    )
    wheel_group.add_argument(
        "--description_file", help="Path to the file with package description"
    )
    wheel_group.add_argument(
        "--description_content_type", help="Content type of the package description"
    )
    wheel_group.add_argument(
        "--entry_points_file",
        help="Path to a correctly-formatted entry_points.txt file",
    )

    contents_group = parser.add_argument_group("Wheel contents")
    contents_group.add_argument(
        "--input_file",
        action="append",
        help="'package_path;real_path' pairs listing "
        "files to be included in the wheel. "
        "Can be supplied multiple times.",
    )
    contents_group.add_argument(
        "--input_file_list",
        action="append",
        help="A file that has all the input files defined as a list to avoid "
        "the long command",
    )
    contents_group.add_argument(
        "--extra_distinfo_file",
        action="append",
        help="'filename;real_path' pairs listing extra files to include in"
        "dist-info directory. Can be supplied multiple times.",
    )
    contents_group.add_argument(
        "--data_files",
        action="append",
        help="'filename;real_path' pairs listing data files to include in"
        "data directory. Can be supplied multiple times.",
    )

    build_group = parser.add_argument_group("Building requirements")
    build_group.add_argument(
        "--volatile_status_file",
        type=Path,
        help="Pass in the stamp info file for stamping",
    )
    build_group.add_argument(
        "--stable_status_file",
        type=Path,
        help="Pass in the stamp info file for stamping",
    )

    return parser.parse_args(sys.argv[1:])


def _parse_file_pairs(content: List[str]) -> List[List[str]]:
    """
    Parse ; delimited lists of files into a 2D list.
    """
    return [i.split(";", maxsplit=1) for i in content or []]


def main() -> None:
    arguments = parse_args()

    input_files = _parse_file_pairs(arguments.input_file)
    extra_distinfo_file = _parse_file_pairs(arguments.extra_distinfo_file)
    data_files = _parse_file_pairs(arguments.data_files)

    for input_file in arguments.input_file_list:
        with open(input_file) as _file:
            input_file_list = _file.read().splitlines()
        for _input_file in input_file_list:
            input_files.append(_input_file.split(";"))

    all_files = get_files_to_package(input_files)
    # Sort the files for reproducible order in the archive.
    all_files = sorted(all_files.items())

    strip_prefixes = [p for p in arguments.strip_path_prefix]

    if arguments.volatile_status_file and arguments.stable_status_file:
        name = resolve_argument_stamp(
            arguments.name,
            arguments.volatile_status_file,
            arguments.stable_status_file,
        )
    else:
        name = arguments.name

    if arguments.volatile_status_file and arguments.stable_status_file:
        version = resolve_argument_stamp(
            arguments.version,
            arguments.volatile_status_file,
            arguments.stable_status_file,
        )
    else:
        version = arguments.version

    with WheelMaker(
        name=name,
        version=version,
        build_tag=arguments.build_tag,
        python_tag=arguments.python_tag,
        abi=arguments.abi,
        platform=arguments.platform,
        outfile=arguments.out,
        strip_path_prefixes=strip_prefixes,
        compress=not arguments.no_compress,
    ) as maker:
        for package_filename, real_filename in all_files:
            maker.add_file(package_filename, real_filename)
        maker.add_wheelfile()

        description = None
        if arguments.description_file:
            with open(
                arguments.description_file, "rt", encoding="utf-8"
            ) as description_file:
                description = description_file.read()

        metadata = arguments.metadata_file.read_text(encoding="utf-8")

        # This is not imported at the top of the file due to the reliance
        # on this file in the `whl_library` repository rule which does not
        # provide `packaging` but does import symbols defined here.
        from packaging.requirements import Requirement

        # Search for any `Requires-Dist` entries that refer to other files and
        # expand them.

        def get_new_requirement_line(reqs_text, extra):
            req = Requirement(reqs_text.strip())
            if req.marker:
                if extra:
                    return f"Requires-Dist: {req.name}{req.specifier}; ({req.marker}) and {extra}"
                else:
                    return f"Requires-Dist: {req.name}{req.specifier}; {req.marker}"
            else:
                return f"Requires-Dist: {req.name}{req.specifier}; {extra}".strip(" ;")

        for meta_line in metadata.splitlines():
            if not meta_line.startswith("Requires-Dist: "):
                continue

            if not meta_line[len("Requires-Dist: ") :].startswith("@"):
                # This is a normal requirement.
                package, _, extra = meta_line[len("Requires-Dist: ") :].rpartition(";")
                if not package:
                    # This is when the package requirement does not have markers.
                    continue
                extra = extra.strip()
                metadata = metadata.replace(
                    meta_line, get_new_requirement_line(package, extra)
                )
                continue

            # This is a requirement that refers to a file.
            file, _, extra = meta_line[len("Requires-Dist: @") :].partition(";")
            extra = extra.strip()

            reqs = []
            for reqs_line in Path(file).read_text(encoding="utf-8").splitlines():
                reqs_text = reqs_line.strip()
                if not reqs_text or reqs_text.startswith(("#", "-")):
                    continue

                # Strip any comments
                reqs_text, _, _ = reqs_text.partition("#")

                reqs.append(get_new_requirement_line(reqs_text, extra))

            metadata = metadata.replace(meta_line, "\n".join(reqs))

        maker.add_metadata(
            metadata=metadata,
            name=name,
            description=description,
        )

        if arguments.entry_points_file:
            maker.add_file(
                maker.distinfo_path("entry_points.txt"), arguments.entry_points_file
            )

        # Sort the files for reproducible order in the archive.
        for filename, real_path in sorted(data_files):
            maker.add_file(maker.data_path(filename), real_path)
        for filename, real_path in sorted(extra_distinfo_file):
            maker.add_file(maker.distinfo_path(filename), real_path)

        maker.add_recordfile()

        # Since stamping may otherwise change the target name of the
        # wheel, the canonical name (with stamps resolved) is written
        # to a file so consumers of the wheel can easily determine
        # the correct name.
        arguments.name_file.write_text(maker.wheelname())


if __name__ == "__main__":
    main()
