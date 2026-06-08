import datetime
import os
import pathlib
import shutil
import tempfile
import unittest
from unittest.mock import patch

from tools.private.release import release as releaser

_UNRELEASED_TEMPLATE = """
<!--
BEGIN_UNRELEASED_TEMPLATE

{#v0-0-0}
## Unreleased

[0.0.0]: https://github.com/bazel-contrib/rules_python/releases/tag/0.0.0

{#v0-0-0-changed}
### Changed
* Nothing changed.

{#v0-0-0-fixed}
### Fixed
* Nothing fixed.

{#v0-0-0-added}
### Added
* Nothing added.

{#v0-0-0-removed}
### Removed
* Nothing removed.

END_UNRELEASED_TEMPLATE
-->
"""


class ReleaserTest(unittest.TestCase):
    def setUp(self):
        self.tmpdir = pathlib.Path(tempfile.mkdtemp())
        self.original_cwd = os.getcwd()
        self.addCleanup(shutil.rmtree, self.tmpdir)

        os.chdir(self.tmpdir)
        # NOTE: On windows, this must be done before files are deleted.
        self.addCleanup(os.chdir, self.original_cwd)

    def test_update_changelog(self):
        changelog = f"""
# Changelog

{_UNRELEASED_TEMPLATE}

{{#v0-0-0}}
## Unreleased

[0.0.0]: https://github.com/bazel-contrib/rules_python/releases/tag/0.0.0

{{#v0-0-0-changed}}
### Changed
* Nothing changed

{{#v0-0-0-fixed}}
### Fixed
* Nothing fixed

{{#v0-0-0-added}}
### Added
* Nothing added

{{#v0-0-0-removed}}
### Removed
* Nothing removed.
"""
        changelog_path = self.tmpdir / "CHANGELOG.md"
        changelog_path.write_text(changelog)

        # Act
        releaser.update_changelog(
            "1.23.4",
            "2025-01-01",
            changelog_path=changelog_path,
        )

        # Assert
        new_content = changelog_path.read_text()

        self.assertIn(
            _UNRELEASED_TEMPLATE, new_content, msg=f"ACTUAL:\n\n{new_content}\n\n"
        )
        self.assertIn(f"## [1.23.4] - 2025-01-01", new_content)
        self.assertIn(
            f"[1.23.4]: https://github.com/bazel-contrib/rules_python/releases/tag/1.23.4",
            new_content,
        )
        self.assertIn("{#v1-23-4}", new_content)
        self.assertIn("{#v1-23-4-changed}", new_content)
        self.assertIn("{#v1-23-4-fixed}", new_content)
        self.assertIn("{#v1-23-4-added}", new_content)
        self.assertIn("{#v1-23-4-removed}", new_content)

    def test_replace_version_next(self):
        # Arrange
        mock_file_content = """
:::{versionadded} VERSION_NEXT_FEATURE
blabla
:::

:::{versionchanged} VERSION_NEXT_PATCH
blabla
:::
"""
        (self.tmpdir / "mock_file.bzl").write_text(mock_file_content)

        releaser.replace_version_next("0.28.0")

        new_content = (self.tmpdir / "mock_file.bzl").read_text()

        self.assertIn(":::{versionadded} 0.28.0", new_content)
        self.assertIn(":::{versionadded} 0.28.0", new_content)
        self.assertNotIn("VERSION_NEXT_FEATURE", new_content)
        self.assertNotIn("VERSION_NEXT_PATCH", new_content)

    def test_replace_version_next_excludes_bazel_dirs(self):
        # Arrange
        mock_file_content = """
:::{versionadded} VERSION_NEXT_FEATURE
blabla
:::
"""
        bazel_dir = self.tmpdir / "bazel-rules_python"
        bazel_dir.mkdir()
        (bazel_dir / "mock_file.bzl").write_text(mock_file_content)

        tools_dir = self.tmpdir / "tools" / "private" / "release"
        tools_dir.mkdir(parents=True)
        (tools_dir / "mock_file.bzl").write_text(mock_file_content)

        tests_dir = self.tmpdir / "tests" / "tools" / "private" / "release"
        tests_dir.mkdir(parents=True)
        (tests_dir / "mock_file.bzl").write_text(mock_file_content)

        version = "0.28.0"

        # Act
        releaser.replace_version_next(version)

        # Assert
        new_content = (bazel_dir / "mock_file.bzl").read_text()
        self.assertIn("VERSION_NEXT_FEATURE", new_content)

        new_content = (tools_dir / "mock_file.bzl").read_text()
        self.assertIn("VERSION_NEXT_FEATURE", new_content)

        new_content = (tests_dir / "mock_file.bzl").read_text()
        self.assertIn("VERSION_NEXT_FEATURE", new_content)

    def test_valid_version(self):
        # These should not raise an exception
        releaser.create_parser().parse_args(["0.28.0"])
        releaser.create_parser().parse_args(["1.0.0"])
        releaser.create_parser().parse_args(["1.2.3rc4"])

    def test_invalid_version(self):
        with self.assertRaises(SystemExit):
            releaser.create_parser().parse_args(["0.28"])
        with self.assertRaises(SystemExit):
            releaser.create_parser().parse_args(["a.b.c"])


class GetLatestVersionTest(unittest.TestCase):
    @patch("tools.private.release.release._get_git_tags")
    def test_get_latest_version_success(self, mock_get_tags):
        mock_get_tags.return_value = ["0.1.0", "1.0.0", "0.2.0"]
        self.assertEqual(releaser.get_latest_version(), "1.0.0")

    @patch("tools.private.release.release._get_git_tags")
    def test_get_latest_version_rc_is_latest(self, mock_get_tags):
        mock_get_tags.return_value = ["0.1.0", "1.0.0", "1.1.0rc0"]
        with self.assertRaisesRegex(
            ValueError, "The latest version is a pre-release version: 1.1.0rc0"
        ):
            releaser.get_latest_version()

    @patch("tools.private.release.release._get_git_tags")
    def test_get_latest_version_no_tags(self, mock_get_tags):
        mock_get_tags.return_value = []
        with self.assertRaisesRegex(
            RuntimeError, "No git tags found matching X.Y.Z or X.Y.ZrcN format."
        ):
            releaser.get_latest_version()

    @patch("tools.private.release.release._get_git_tags")
    def test_get_latest_version_no_matching_tags(self, mock_get_tags):
        mock_get_tags.return_value = ["v1.0", "latest"]
        with self.assertRaisesRegex(
            RuntimeError, "No git tags found matching X.Y.Z or X.Y.ZrcN format."
        ):
            releaser.get_latest_version()

    @patch("tools.private.release.release._get_git_tags")
    def test_get_latest_version_only_rc_tags(self, mock_get_tags):
        mock_get_tags.return_value = ["1.0.0rc0", "1.1.0rc0"]
        with self.assertRaisesRegex(
            ValueError, "The latest version is a pre-release version: 1.1.0rc0"
        ):
            releaser.get_latest_version()


class DetermineNextVersionTest(unittest.TestCase):
    def setUp(self):
        self.tmpdir = pathlib.Path(tempfile.mkdtemp())
        self.original_cwd = os.getcwd()
        self.addCleanup(shutil.rmtree, self.tmpdir)

        os.chdir(self.tmpdir)
        # NOTE: On windows, this must be done before files are deleted.
        self.addCleanup(os.chdir, self.original_cwd)

        self.mock_get_latest_version = patch(
            "tools.private.release.release.get_latest_version"
        ).start()
        self.addCleanup(patch.stopall)

    def test_no_markers(self):
        (self.tmpdir / "mock_file.bzl").write_text("no markers here")
        self.mock_get_latest_version.return_value = "1.2.3"

        next_version = releaser.determine_next_version()

        self.assertEqual(next_version, "1.2.4")

    def test_only_patch(self):
        (self.tmpdir / "mock_file.bzl").write_text(
            ":::{versionchanged} VERSION_NEXT_PATCH"
        )
        self.mock_get_latest_version.return_value = "1.2.3"

        next_version = releaser.determine_next_version()

        self.assertEqual(next_version, "1.2.4")

    def test_only_feature(self):
        (self.tmpdir / "mock_file.bzl").write_text(
            ":::{versionadded} VERSION_NEXT_FEATURE"
        )
        self.mock_get_latest_version.return_value = "1.2.3"

        next_version = releaser.determine_next_version()

        self.assertEqual(next_version, "1.3.0")

    def test_both_markers(self):
        (self.tmpdir / "mock_file_patch.bzl").write_text(
            ":::{versionchanged} VERSION_NEXT_PATCH"
        )
        (self.tmpdir / "mock_file_feature.bzl").write_text(
            ":::{versionadded} VERSION_NEXT_FEATURE"
        )
        self.mock_get_latest_version.return_value = "1.2.3"

        next_version = releaser.determine_next_version()

        self.assertEqual(next_version, "1.3.0")


if __name__ == "__main__":
    unittest.main()
