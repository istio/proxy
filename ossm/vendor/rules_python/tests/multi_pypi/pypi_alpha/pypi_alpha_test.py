import sys

from more_itertools import __version__

if __name__ == "__main__":
    expected_version = "9.1.0"
    if __version__ != expected_version:
        sys.exit(f"Expected version {expected_version}, got {__version__}")
