"""Tests the layer created by `oci_base_edit` (`:base_edit`)."""

import sys
import tarfile
import unittest

LAYER = sys.argv.pop(1)


class BaseEditTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        with tarfile.open(LAYER) as tar:
            cls.members = {m.name.removeprefix("./"): m for m in tar.getmembers()}
            cls.files = {
                name: tar.extractfile(m).read().decode()
                for name, m in cls.members.items()
                if m.isfile()}

    def assert_member(self, name, mode, uid=0, gid=0, directory=False):
        member = self.members[name]
        self.assertEqual(member.isdir(), directory, name)
        self.assertEqual(oct(member.mode), oct(mode), name)
        self.assertEqual((member.uid, member.gid), (uid, gid), name)

    def test_contents(self):
        # The edited files and the new home (and its new parent). The directories that are in the
        # image (`etc`, `var/lib/dpkg`, ...) are not in the layer, so keep their metadata.
        self.assertEqual(
            sorted(self.members),
            sorted([
                "etc/passwd", "etc/group", "etc/shadow", "etc/gshadow", "home", "home/envoy",
                "var/lib/dpkg/status"
            ]))

    def test_passwd(self):
        # Base entries are kept (the base file has no final newline), and users are added with
        # the first free system ids, `envoy` with its own group of the same id (101: uid and gid
        # 100 are used).
        self.assertEqual(
            self.files["etc/passwd"].splitlines(), [
                "root:x:0:0:root:/root:/bin/bash",
                "daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin",
                "nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin",
                "_apt:x:100:65534::/nonexistent:/usr/sbin/nologin",
                "envoy:x:101:101::/home/envoy:/usr/sbin/nologin",
                "worker:x:102:100::/home/worker:/usr/sbin/nologin",
            ])
        self.assert_member("etc/passwd", 0o644)

    def test_group(self):
        # The group file of the second layer (which also deletes that of the first), with the
        # groups added.
        self.assertEqual(
            self.files["etc/group"].splitlines(), [
                "root:x:0:",
                "daemon:x:1:",
                "shadow:x:42:",
                "users:x:100:",
                "nogroup:x:65534:",
                "proxy:x:200:",
                "envoy:x:101:",
            ])
        self.assert_member("etc/group", 0o644)

    def test_shadow(self):
        # New entries take the date of `root`, and the files keep the `shadow` group.
        self.assertEqual(
            self.files["etc/shadow"].splitlines()[-2:], [
                "envoy:*:20000:0:99999:7:::",
                "worker:*:20000:0:99999:7:::",
            ])
        self.assertEqual(
            self.files["etc/gshadow"].splitlines()[-2:], [
                "proxy:!::",
                "envoy:!::",
            ])
        self.assert_member("etc/shadow", 0o640, gid=42)
        self.assert_member("etc/gshadow", 0o640, gid=42)

    def test_home(self):
        self.assert_member("home", 0o755, directory=True)
        self.assert_member("home/envoy", 0o750, uid=101, gid=101, directory=True)

    def test_dpkg_status(self):
        entries = [
            dict(line.split(": ", 1) for line in entry.splitlines())
            for entry in self.files["var/lib/dpkg/status"].strip().split("\n\n")
        ]
        self.assertEqual(
            [(e["Package"], e["Version"], e["Status"]) for e in entries], [
                # The status of the third layer, which replaces `/var/lib/dpkg` (opaque).
                ("base-files", "12ubuntu4.1", "install ok installed"),
                ("ca-certificates", "20260816ubuntu0.22.04.1", "install ok installed"),
                ("tzdata", "2026e-0ubuntu0.22.04.1", "install ok installed"),
            ])
        self.assert_member("var/lib/dpkg/status", 0o644)


if __name__ == "__main__":
    unittest.main()
