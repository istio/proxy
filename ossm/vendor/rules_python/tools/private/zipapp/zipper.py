import argparse
import os
import shutil
import stat
import sys
import zipfile

# Unix permission bit for symlink (S_IFLNK)
# S_IFLNK is usually 0o120000
S_IFLNK = 0o120000


def _get_zip_runfiles_path(
    path, workspace_name, legacy_external_runfiles, runfiles_dir
):
    if legacy_external_runfiles and path.startswith("external/"):
        path = path[len("external/") :]
    elif path.startswith("../"):
        path = path[3:]
    else:
        path = os.path.join(workspace_name, path)
    return os.path.join(runfiles_dir, path)


def _parse_entry(
    line,
    line_idx,
    workspace_name,
    legacy_external_runfiles,
    runfiles_dir,
):
    line = line.strip()
    if not line:
        return None

    parts = line.split("|")
    type_ = parts[0]

    if type_ == "regular":
        _, is_symlink_str, zip_path, content_path = parts
    elif type_ == "rf-empty":
        _, runfile_path = parts
        zip_path = _get_zip_runfiles_path(
            runfile_path, workspace_name, legacy_external_runfiles, runfiles_dir
        )
        content_path = None  # Empty file
        is_symlink_str = "0"
    elif type_ == "rf-file":
        _, is_symlink_str, runfile_path, content_path = parts
        zip_path = _get_zip_runfiles_path(
            runfile_path, workspace_name, legacy_external_runfiles, runfiles_dir
        )
    elif type_ == "rf-symlink":
        _, is_symlink_str, runfile_path, content_path = parts
        zip_path = os.path.join(runfiles_dir, workspace_name, runfile_path)
    elif type_ == "rf-root-symlink":
        _, is_symlink_str, runfile_path, content_path = parts
        zip_path = os.path.join(runfiles_dir, runfile_path)
    else:
        raise ValueError(
            f"Error: Unknown entry type or invalid format at line {line_idx + 1}: {line}"
        )

    return type_, is_symlink_str, zip_path, content_path


def read_manifest(
    manifest_path, workspace_name, legacy_external_runfiles, runfiles_dir
):
    with open(manifest_path, "r") as f:
        entries = []
        for line_idx, line in enumerate(f):
            try:
                entry = _parse_entry(
                    line,
                    line_idx,
                    workspace_name,
                    legacy_external_runfiles,
                    runfiles_dir,
                )
                if entry:
                    entries.append(entry)
            except ValueError as e:
                e.add_note(f"Error processing line {line_idx + 1}: {line.strip()}")
                raise

    # Sort by zip path (3rd element in tuple)
    entries.sort(key=lambda x: x[2])
    return entries


def _write_entry(zf, entry, compress_type):
    type_, is_symlink_str, zip_path, content_path = entry

    if type_ == "rf-empty":
        zi = zipfile.ZipInfo(zip_path)
        zi.date_time = (1980, 1, 1, 0, 0, 0)
        zi.create_system = 3  # Unix
        zi.compress_type = compress_type
        # Create empty file
        zi.external_attr = (0o644 & 0xFFFF) << 16
        zf.writestr(zi, "")
        return

    if is_symlink_str == "-1":
        if not os.path.exists(content_path):
            is_symlink_str = "1"
        else:
            is_symlink_str = "0"

    is_symlink = is_symlink_str == "1"

    if is_symlink:
        zi = zipfile.ZipInfo(zip_path)
        zi.date_time = (1980, 1, 1, 0, 0, 0)
        zi.create_system = 3  # Unix
        zi.compress_type = compress_type
        target = os.readlink(content_path)
        # Set permissions to 777 for symlink (standard)
        zi.external_attr = (S_IFLNK | 0o777) << 16
        zf.writestr(zi, target)
    else:
        st = os.stat(content_path)
        zi = zipfile.ZipInfo(zip_path)
        zi.date_time = (1980, 1, 1, 0, 0, 0)
        zi.create_system = 3  # Unix
        zi.compress_type = compress_type
        # Preserve permissions, otherwise execute is dropped.
        zi.external_attr = (st.st_mode & 0xFFFF) << 16
        with open(content_path, "rb") as src, zf.open(zi, "w") as dst:
            shutil.copyfileobj(src, dst)


def create_zip(
    *,
    manifest_path,
    output_zip,
    compress_level,
    workspace_name,
    legacy_external_runfiles,
    runfiles_dir,
):
    compress_type = zipfile.ZIP_STORED if compress_level == 0 else zipfile.ZIP_DEFLATED
    zf_level = compress_level if compress_level != 0 else None

    entries = read_manifest(
        manifest_path, workspace_name, legacy_external_runfiles, runfiles_dir
    )

    with zipfile.ZipFile(
        output_zip, "w", compress_type, allowZip64=True, compresslevel=zf_level
    ) as zf:
        for entry in entries:
            _write_entry(zf, entry, compress_type)


def main():
    parser = argparse.ArgumentParser(description="Create a zip file from a manifest.")
    parser.add_argument(
        "manifest",
        help="""
Path to the manifest file. Lines have one of the following formats:

1. `regular|is_symlink|zip_path|content_path`: This form stores the `zip_path`
   in the zip, whose content is taken from `content_path`

2. `rf-empty|runfile_path`: A  `runfiles.empty_filenames` value. The stored
   zip path is computed from `runfile_path`

3. `rf-file|is_symlink|runfile_path|content_path`: Store a file in
   the zip. The zip path is computed from `runfile_path`.

4. `rf-symlink|is_symlink|runfile_symlink_path|content_path`: Store a
   main-repo-relative path in the zip.

5. `rf-root-symlink|is_symlink|runfile_root_path|content_path`: Store a
   runfiles-root-relative path in the zip.

In all cases, `is_symlink` has the following values:
* `1` means it should be stored as a symlink whose value is read
  (using `readlink()`) from `content_path`.
* `0` means to store it as a regular file, read from `content_path`
* `-1` occurs with Bazel 7 (because it lacks `File.is_symlink`), which means
  to infer whether it's a symlink (files to be stored as symlinks can be
  determined by looking for symlinks that point to non-existent files).

For runfiles entries, they have `--runfiles-dir` prepended to their computed
zip path.

Compute `zip_path` from `runfile_path`: Computing the final zip path for
runfiles entries is a bit complicated, but boils down to computing what the
runfiles-root-relative path would be, with `--legacy-external-runfiles` taken
into account.
""",
    )
    parser.add_argument("output", help="Path to the output zip file.")
    parser.add_argument(
        "--compression",
        type=int,
        default=0,
        help="Compression level (0 for stored, others for deflated)",
    )
    parser.add_argument("--workspace-name", default="", help="Name of the workspace")
    parser.add_argument(
        "--legacy-external-runfiles",
        default="0",
        choices=["0", "1"],
        help="Whether to use legacy external runfiles behavior",
    )
    parser.add_argument(
        "--runfiles-dir", default="runfiles", help="Name of the runfiles directory"
    )
    args = parser.parse_args()

    try:
        create_zip(
            manifest_path=args.manifest,
            output_zip=args.output,
            compress_level=args.compression,
            workspace_name=args.workspace_name,
            legacy_external_runfiles=args.legacy_external_runfiles == "1",
            runfiles_dir=args.runfiles_dir,
        )
    except Exception as e:
        e.add_note(f"Error creating zip {args.output}")
        raise


if __name__ == "__main__":
    sys.exit(main())
