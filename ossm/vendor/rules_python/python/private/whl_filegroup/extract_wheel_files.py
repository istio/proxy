"""Extract files from a wheel's RECORD."""

import csv
import re
import sys
import zipfile
from collections.abc import Iterable
from pathlib import Path

WhlRecord = Iterable[str]


def get_record(whl_path: Path) -> WhlRecord:
    try:
        zipf = zipfile.ZipFile(whl_path)
    except zipfile.BadZipFile as ex:
        raise RuntimeError(f"{whl_path} is not a valid zip file") from ex
    files = zipf.namelist()
    try:
        (record_file,) = [name for name in files if name.endswith(".dist-info/RECORD")]
    except ValueError:
        raise RuntimeError(f"{whl_path} doesn't contain exactly one .dist-info/RECORD")
    record_lines = zipf.read(record_file).decode().splitlines()
    return (row[0] for row in csv.reader(record_lines))


def get_files(whl_record: WhlRecord, regex_pattern: str) -> list[str]:
    """Get files in a wheel that match a regex pattern."""
    p = re.compile(regex_pattern)
    return [filepath for filepath in whl_record if re.match(p, filepath)]


def extract_files(whl_path: Path, files: Iterable[str], outdir: Path) -> None:
    """Extract files from whl_path to outdir."""
    zipf = zipfile.ZipFile(whl_path)
    for file in files:
        zipf.extract(file, outdir)


def main() -> None:
    if len(sys.argv) not in {3, 4}:
        print(
            f"Usage: {sys.argv[0]} <wheel> <out_dir> [regex_pattern]",
            file=sys.stderr,
        )
        sys.exit(1)

    whl_path = Path(sys.argv[1]).resolve()
    outdir = Path(sys.argv[2])
    regex_pattern = sys.argv[3] if len(sys.argv) == 4 else ""

    whl_record = get_record(whl_path)
    files = get_files(whl_record, regex_pattern)
    extract_files(whl_path, files, outdir)


if __name__ == "__main__":
    main()
