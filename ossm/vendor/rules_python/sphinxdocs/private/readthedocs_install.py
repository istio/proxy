import os
import pathlib
import shutil
import sys

from python import runfiles


def main(args):
    if not args:
        raise ValueError("Empty args: expected paths to copy")

    if not (install_to := os.environ.get("READTHEDOCS_OUTPUT")):
        raise ValueError("READTHEDOCS_OUTPUT environment variable not set")

    install_to = pathlib.Path(install_to)

    rf = runfiles.Create()
    for doc_dir_runfiles_path in args:
        doc_dir_path = pathlib.Path(rf.Rlocation(doc_dir_runfiles_path))
        dest = install_to / doc_dir_path.name
        print(f"Copying {doc_dir_path} to {dest}")
        shutil.copytree(src=doc_dir_path, dst=dest, dirs_exist_ok=True)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
