"""Copy a generated file to the source tree.

Run like:
    copy_to_source path/to/generated_file path/to/source_file_to_overwrite
"""

import os
import shutil
import stat
import sys
from pathlib import Path


def copy_to_source(generated_relative_path: Path, target_relative_path: Path) -> None:
    """Copy the generated file to the target file path.

    Expands the relative paths by looking at Bazel env vars to figure out which absolute paths to use.
    """
    # This script normally gets executed from the runfiles dir, so find the absolute path to the generated file based on that.
    generated_absolute_path = Path.cwd() / generated_relative_path

    # Similarly, the target is relative to the source directory.
    target_absolute_path = os.getenv("BUILD_WORKSPACE_DIRECTORY") / target_relative_path

    print(f"Copying {generated_absolute_path} to {target_absolute_path}")
    target_absolute_path.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy(generated_absolute_path, target_absolute_path)

    target_absolute_path.chmod(0o664)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("Usage: copy_to_source <generated_file> <target_file>")

    copy_to_source(Path(sys.argv[1]), Path(sys.argv[2]))
