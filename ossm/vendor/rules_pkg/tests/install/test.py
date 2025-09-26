#!/usr/bin/env python3

# Copyright 2021 The Bazel Authors. All rights reserved.
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

import os
import pathlib
import unittest
import stat
import subprocess

from python.runfiles import runfiles
from pkg.private import manifest


class PkgInstallTestBase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.runfiles = runfiles.Create()
        # Somewhat of an implementation detail, but it works.  I think.
        manifest_file = cls.runfiles.Rlocation("rules_pkg/tests/install/test_installer_install_script-install-manifest.json")

        with open(manifest_file, 'r') as fh:
            manifest_entries = manifest.read_entries_from(fh)
            cls.manifest_data = {}

            for entry in manifest_entries:
                cls.manifest_data[pathlib.Path(entry.dest)] = entry
        cls.installdir = pathlib.Path(os.getenv("TEST_TMPDIR")) / "installdir"


class PkgInstallTest(PkgInstallTestBase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        env = {}
        env.update(cls.runfiles.EnvVars())
        subprocess.check_call([
            cls.runfiles.Rlocation("rules_pkg/tests/install/test_installer"),
            "--destdir", cls.installdir,
            "--verbose",
        ],
                              env=env)

    def entity_type_at_path(self, path):
        if path.is_symlink():
            return manifest.ENTRY_IS_LINK
        elif path.is_file():
            return manifest.ENTRY_IS_FILE
        elif path.is_dir():
            return manifest.ENTRY_IS_DIR
        else:
            # We can't infer what TreeArtifacts are by looking at them -- the
            # build system is not aware of their contents.
            raise ValueError("Entity {} is not a link, file, or directory")

    def assertEntryTypeMatches(self, entry, actual_path):
        actual_entry_type = self.entity_type_at_path(actual_path)

        # TreeArtifacts looks like directories.
        if (entry.type == manifest.ENTRY_IS_TREE and
                actual_entry_type == manifest.ENTRY_IS_DIR):
            return

        self.assertEqual(actual_entry_type, entry.type,
                        "Entity {} should be a {}, but was actually {}".format(
                            entry.dest,
                            manifest.entry_type_to_string(entry.type),
                            manifest.entry_type_to_string(actual_entry_type),
                        ))

    def assertEntryModeMatches(self, entry, actual_path,
                               is_tree_artifact_content=False):
        # TODO: permissions in windows are... tricky.  Don't bother
        # testing for them if we're in it for the time being
        if os.name == 'nt':
            return

        actual_mode = stat.S_IMODE(os.stat(actual_path).st_mode)
        expected_mode = int(entry.mode, 8)

        if (not is_tree_artifact_content and
                entry.type == manifest.ENTRY_IS_TREE):
            expected_mode |= 0o555

        self.assertEqual(actual_mode, expected_mode,
            "Entry {}{} has mode {:04o}, expected {:04o}".format(
            entry.dest,
            f" ({actual_path})" if is_tree_artifact_content else "",
            actual_mode, expected_mode,
        ))

    def _find_tree_entry(self, path, owned_trees):
        for tree_root in owned_trees:
            if self._path_starts_with(path, tree_root):
                return tree_root
        return None

    def _path_starts_with(self, path, other):
        return path.parts[:len(other.parts)] == other.parts

    def test_manifest_matches(self):
        unowned_dirs = set()
        owned_dirs = set()
        owned_trees = dict()

        # Figure out what directories we are supposed to own, and which ones we
        # aren't.
        #
        # Unowned directories are created implicitly by requesting other
        # elements be created or installed.
        #
        # Owned directories are created explicitly with the pkg_mkdirs rule.
        for dest, data in self.manifest_data.items():
            if data.type == manifest.ENTRY_IS_DIR:
                owned_dirs.add(dest)
            elif data.type == manifest.ENTRY_IS_TREE:
                owned_trees[dest] = data

            unowned_dirs.update(dest.parents)

        # In the above loop, unowned_dirs contains all possible directories that
        # are in the manifest.  Prune them here.
        unowned_dirs -= owned_dirs

        # TODO: check for ownership (user, group)
        found_entries = {dest: False for dest in self.manifest_data}
        for root, dirs, files in os.walk(self.installdir):
            root = pathlib.Path(root)
            rel_root_path = root.relative_to(self.installdir)

            # Directory ownership tests
            if len(files) == 0 and len(dirs) == 0:
                # Empty directories must be explicitly requested by something
                if rel_root_path not in self.manifest_data:
                    self.fail("Directory {} not in manifest".format(rel_root_path))

                entry = self.manifest_data[rel_root_path]
                self.assertEntryTypeMatches(entry, root)
                self.assertEntryModeMatches(entry, root)

                found_entries[rel_root_path] = True
            else:
                # There's something in here.  Depending on how it was set up, it
                # could either be owned or unowned.
                if rel_root_path in self.manifest_data:
                    entry = self.manifest_data[rel_root_path]
                    self.assertEntryTypeMatches(entry, root)
                    self.assertEntryModeMatches(entry, root)

                    found_entries[rel_root_path] = True
                else:
                    # If any unowned directories are here, they must be the
                    # prefix of some entity in the manifest.
                    is_unowned = rel_root_path in unowned_dirs
                    is_tree_intermediate_dir = bool(
                        self._find_tree_entry(rel_root_path, owned_trees))
                    self.assertTrue(is_unowned or is_tree_intermediate_dir)

            for f in files:
                # The path on the filesystem in which the file actually exists.

                # TODO(#382): This part of the test assumes that the path
                # separator is '/', which is not the case in Windows.  However,
                # paths emitted in the JSON manifests may also be using
                # '/'-separated paths.
                #
                # Confirm the degree to which this is a problem, and remedy as
                # needed.  It maybe worth setting the keys in the manifest_data
                # dictionary to pathlib.Path or otherwise converting them to
                # native paths.
                fpath = root / f
                # The path inside the manifest (relative to the install
                # destdir).
                rel_fpath = rel_root_path / f
                entity_in_manifest = rel_fpath in self.manifest_data
                entity_tree_root = self._find_tree_entry(rel_fpath, owned_trees)
                if not entity_in_manifest and not entity_tree_root:
                    self.fail("Entity {} not in manifest".format(rel_fpath))

                if entity_in_manifest:
                    entry = self.manifest_data[rel_fpath]
                    self.assertEntryTypeMatches(entry, fpath)
                    self.assertEntryModeMatches(entry, fpath)

                if entity_tree_root:
                    entry = owned_trees[entity_tree_root]
                    self.assertEntryModeMatches(entry, fpath,
                                                is_tree_artifact_content=True)

                found_entries[rel_fpath] = True

        num_missing = 0
        for dest, present in found_entries.items():
            if present is False:
                print("Entity {} is missing from the tree".format(dest))
                num_missing += 1
        self.assertEqual(num_missing, 0)


class WipeTest(PkgInstallTestBase):
    def test_wipe(self):
        self.installdir.mkdir(exist_ok=True)
        (self.installdir / "should_be_deleted.txt").touch()

        subprocess.check_call([
            self.runfiles.Rlocation("rules_pkg/tests/install/test_installer"),
            "--destdir", self.installdir,
            "--wipe_destdir",
        ],
                              env=self.runfiles.EnvVars())
        self.assertFalse((self.installdir / "should_be_deleted.txt").exists())


if __name__ == "__main__":
    unittest.main()
