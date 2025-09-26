import unittest
from random import shuffle

from python.private.pypi.whl_installer.platform import (
    OS,
    Arch,
    Platform,
    host_interpreter_version,
)


class MinorVersionTest(unittest.TestCase):
    def test_host(self):
        host = host_interpreter_version()
        self.assertIsNotNone(host)


class PlatformTest(unittest.TestCase):
    def test_can_get_host(self):
        host = Platform.host()
        self.assertIsNotNone(host)
        self.assertEqual(1, len(Platform.from_string("host")))
        self.assertEqual(host, Platform.from_string("host"))

    def test_can_get_linux_x86_64_without_py_version(self):
        got = Platform.from_string("linux_x86_64")
        want = Platform(os=OS.linux, arch=Arch.x86_64)
        self.assertEqual(want, got[0])

    def test_can_get_specific_from_string(self):
        got = Platform.from_string("cp33_linux_x86_64")
        want = Platform(os=OS.linux, arch=Arch.x86_64, minor_version=3)
        self.assertEqual(want, got[0])

        got = Platform.from_string("cp33.0_linux_x86_64")
        want = Platform(os=OS.linux, arch=Arch.x86_64, minor_version=3, micro_version=0)
        self.assertEqual(want, got[0])

    def test_can_get_all_for_py_version(self):
        cp39 = Platform.all(minor_version=9, micro_version=0)
        self.assertEqual(21, len(cp39), f"Got {cp39}")
        self.assertEqual(cp39, Platform.from_string("cp39.0_*"))

    def test_can_get_all_for_os(self):
        linuxes = Platform.all(OS.linux, minor_version=9)
        self.assertEqual(7, len(linuxes))
        self.assertEqual(linuxes, Platform.from_string("cp39_linux_*"))

    def test_can_get_all_for_os_for_host_python(self):
        linuxes = Platform.all(OS.linux)
        self.assertEqual(7, len(linuxes))
        self.assertEqual(linuxes, Platform.from_string("linux_*"))

    def test_platform_sort(self):
        platforms = [
            Platform(os=OS.linux, arch=None),
            Platform(os=OS.linux, arch=Arch.x86_64),
            Platform(os=OS.osx, arch=None),
            Platform(os=OS.osx, arch=Arch.x86_64),
            Platform(os=OS.osx, arch=Arch.aarch64),
        ]
        shuffle(platforms)
        platforms.sort()
        want = [
            Platform(os=OS.linux, arch=None),
            Platform(os=OS.linux, arch=Arch.x86_64),
            Platform(os=OS.osx, arch=None),
            Platform(os=OS.osx, arch=Arch.x86_64),
            Platform(os=OS.osx, arch=Arch.aarch64),
        ]

        self.assertEqual(want, platforms)

    def test_wheel_os_alias(self):
        self.assertEqual("osx", str(OS.osx))
        self.assertEqual(str(OS.darwin), str(OS.osx))

    def test_wheel_arch_alias(self):
        self.assertEqual("x86_64", str(Arch.x86_64))
        self.assertEqual(str(Arch.amd64), str(Arch.x86_64))

    def test_wheel_platform_alias(self):
        give = Platform(
            os=OS.darwin,
            arch=Arch.amd64,
        )
        alias = Platform(
            os=OS.osx,
            arch=Arch.x86_64,
        )

        self.assertEqual("osx_x86_64", str(give))
        self.assertEqual(str(alias), str(give))


if __name__ == "__main__":
    unittest.main()
