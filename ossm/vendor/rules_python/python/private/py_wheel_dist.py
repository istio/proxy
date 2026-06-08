"""A utility for generating the output directory for `py_wheel_dist`."""

import argparse
import shutil
from pathlib import Path


def parse_args() -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--wheel", type=Path, required=True, help="The path to a wheel."
    )
    parser.add_argument(
        "--name_file",
        type=Path,
        required=True,
        help="A file containing the sanitized name of the wheel.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="The output location to copy the wheel to.",
    )

    return parser.parse_args()


def main() -> None:
    """The main entrypoint."""
    args = parse_args()

    wheel_name = args.name_file.read_text(encoding="utf-8").strip()
    args.output.mkdir(exist_ok=True, parents=True)
    shutil.copyfile(args.wheel, args.output / wheel_name)


if __name__ == "__main__":
    main()
