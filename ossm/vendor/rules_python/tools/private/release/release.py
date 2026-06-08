"""A tool to perform release steps."""

import argparse
import datetime
import fnmatch
import os
import pathlib
import re
import subprocess

from packaging.version import parse as parse_version

_EXCLUDE_PATTERNS = [
    "./.git/*",
    "./.github/*",
    "./.bazelci/*",
    "./.bcr/*",
    "./bazel-*/*",
    "./CONTRIBUTING.md",
    "./RELEASING.md",
    "./tools/private/release/*",
    "./tests/tools/private/release/*",
]


def _iter_version_placeholder_files():
    for root, dirs, files in os.walk(".", topdown=True):
        # Filter directories
        dirs[:] = [
            d
            for d in dirs
            if not any(
                fnmatch.fnmatch(os.path.join(root, d), pattern)
                for pattern in _EXCLUDE_PATTERNS
            )
        ]

        for filename in files:
            filepath = os.path.join(root, filename)
            if any(fnmatch.fnmatch(filepath, pattern) for pattern in _EXCLUDE_PATTERNS):
                continue

            yield filepath


def _get_git_tags():
    """Runs a git command and returns the output."""
    return subprocess.check_output(["git", "tag"]).decode("utf-8").splitlines()


def get_latest_version():
    """Gets the latest version from git tags."""
    tags = _get_git_tags()
    # The packaging module can parse PEP440 versions, including RCs.
    # It has a good understanding of version precedence.
    versions = [
        (tag, parse_version(tag))
        for tag in tags
        if re.match(r"^\d+\.\d+\.\d+(rc\d+)?$", tag.strip())
    ]
    if not versions:
        raise RuntimeError("No git tags found matching X.Y.Z or X.Y.ZrcN format.")

    versions.sort(key=lambda v: v[1])
    latest_tag, latest_version = versions[-1]

    if latest_version.is_prerelease:
        raise ValueError(f"The latest version is a pre-release version: {latest_tag}")

    # After all that, we only want to consider stable versions for the release.
    stable_versions = [tag for tag, version in versions if not version.is_prerelease]
    if not stable_versions:
        raise ValueError("No stable git tags found matching X.Y.Z format.")

    # The versions are already sorted, so the last one is the latest.
    return stable_versions[-1]


def should_increment_minor():
    """Checks if the minor version should be incremented."""
    for filepath in _iter_version_placeholder_files():
        try:
            with open(filepath, "r") as f:
                content = f.read()
        except (IOError, UnicodeDecodeError):
            # Ignore binary files or files with read errors
            continue

        if "VERSION_NEXT_FEATURE" in content:
            return True
    return False


def determine_next_version():
    """Determines the next version based on git tags and placeholders."""
    latest_version = get_latest_version()
    major, minor, patch = [int(n) for n in latest_version.split(".")]

    if should_increment_minor():
        return f"{major}.{minor + 1}.0"
    else:
        return f"{major}.{minor}.{patch + 1}"


def update_changelog(version, release_date, changelog_path="CHANGELOG.md"):
    """Performs the version replacements in CHANGELOG.md."""

    header_version = version.replace(".", "-")

    changelog_path_obj = pathlib.Path(changelog_path)
    lines = changelog_path_obj.read_text().splitlines()

    new_lines = []
    after_template = False
    before_already_released = True
    for line in lines:
        if "END_UNRELEASED_TEMPLATE" in line:
            after_template = True
        if re.match("#v[1-9]-", line):
            before_already_released = False

        if after_template and before_already_released:
            line = line.replace("## Unreleased", f"## [{version}] - {release_date}")
            line = line.replace("v0-0-0", f"v{header_version}")
            line = line.replace("0.0.0", version)

        new_lines.append(line)

    changelog_path_obj.write_text("\n".join(new_lines))


def replace_version_next(version):
    """Replaces all VERSION_NEXT_* placeholders with the new version."""
    for filepath in _iter_version_placeholder_files():
        try:
            with open(filepath, "r") as f:
                content = f.read()
        except (IOError, UnicodeDecodeError):
            # Ignore binary files or files with read errors
            continue

        if "VERSION_NEXT_FEATURE" in content or "VERSION_NEXT_PATCH" in content:
            new_content = content.replace("VERSION_NEXT_FEATURE", version)
            new_content = new_content.replace("VERSION_NEXT_PATCH", version)
            with open(filepath, "w") as f:
                f.write(new_content)


def _semver_type(value):
    if not re.match(r"^\d+\.\d+\.\d+(rc\d+)?$", value):
        raise argparse.ArgumentTypeError(
            f"'{value}' is not a valid semantic version (X.Y.Z or X.Y.ZrcN)"
        )
    return value


def create_parser():
    """Creates the argument parser."""
    parser = argparse.ArgumentParser(
        description="Automate release steps for rules_python."
    )
    parser.add_argument(
        "version",
        nargs="?",
        type=_semver_type,
        help="The new release version (e.g., 0.28.0). If not provided, "
        "it will be determined automatically.",
    )
    return parser


def main():
    # Change to the workspace root so the script can be run using `bazel run`
    if "BUILD_WORKSPACE_DIRECTORY" in os.environ:
        os.chdir(os.environ["BUILD_WORKSPACE_DIRECTORY"])

    parser = create_parser()
    args = parser.parse_args()

    version = args.version
    if version is None:
        print("No version provided, determining next version automatically...")
        version = determine_next_version()
        print(f"Determined next version: {version}")

    print("Updating changelog ...")
    release_date = datetime.date.today().strftime("%Y-%m-%d")
    update_changelog(version, release_date)

    print("Replacing VERSION_NEXT placeholders ...")
    replace_version_next(version)

    print("Done")


if __name__ == "__main__":
    main()
