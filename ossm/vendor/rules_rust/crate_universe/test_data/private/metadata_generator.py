#!/usr/bin/env python3

import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import List


def run_subprocess(command: List[str]) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(command, capture_output=True, check=False)

    if proc.returncode:
        print("Subcommand exited with error", proc.returncode, file=sys.stderr)
        print("Args:", proc.args, file=sys.stderr)
        print("stderr:", proc.stderr.decode("utf-8"), file=sys.stderr)
        print("stdout:", proc.stdout.decode("utf-8"), file=sys.stderr)
        exit(proc.returncode)

    return proc


def main() -> None:
    """The main entrypoint."""
    workspace_root = Path(
        os.environ.get(
            "BUILD_WORKSPACE_DIRECTORY",
            str(Path(__file__).parent.parent.parent.parent.parent),
        )
    )
    metadata_dir = workspace_root / "crate_universe/test_data/metadata"
    cargo = os.getenv("CARGO", "cargo")

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_dir_path = Path(temp_dir)
        temp_dir_path.mkdir(parents=True, exist_ok=True)

        for test_dir in metadata_dir.iterdir():

            # Check to see if the directory contains a Cargo manifest
            real_manifest = test_dir / "Cargo.toml"
            if not real_manifest.exists():
                continue

            # Copy the test directory into a temp directory (and out from under a Cargo workspace)
            manifest_dir = temp_dir_path / test_dir.name
            shutil.copytree(test_dir, manifest_dir)

            manifest = manifest_dir / "Cargo.toml"
            lockfile = manifest_dir / "Cargo.lock"

            if lockfile.exists():
                proc = run_subprocess(
                    [cargo, "update", "--manifest-path", str(manifest), "--workspace"]
                )
            else:
                # Generate Lockfile
                proc = run_subprocess(
                    [cargo, "generate-lockfile", "--manifest-path", str(manifest)]
                )

                if not lockfile.exists():
                    print("Faield to generate lockfile")
                    print("Args:", proc.args, file=sys.stderr)
                    print("stderr:", proc.stderr.decode("utf-8"), file=sys.stderr)
                    print("stdout:", proc.stdout.decode("utf-8"), file=sys.stderr)
                    exit(1)

            shutil.copy2(str(lockfile), str(test_dir / "Cargo.lock"))

            # Generate metadata
            proc = run_subprocess(
                [
                    cargo,
                    "metadata",
                    "--format-version",
                    "1",
                    "--manifest-path",
                    str(manifest),
                ]
            )

            cargo_home = os.environ.get("CARGO_HOME", str(Path.home() / ".cargo"))

            # Replace the temporary directory so package IDs are predictable
            metadata_text = proc.stdout.decode("utf-8")
            metadata_text = metadata_text.replace(temp_dir, "{TEMP_DIR}")
            metadata_text = metadata_text.replace(cargo_home, "{CARGO_HOME}")

            # Write metadata to disk
            metadata = json.loads(metadata_text)
            output = test_dir / "metadata.json"
            output.write_text(json.dumps(metadata, indent=4, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
