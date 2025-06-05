import unittest
from unittest import mock

from python.private.pypi.whl_installer import wheel
from python.private.pypi.whl_installer.platform import OS, Arch, Platform

_HOST_INTERPRETER_FN = (
    "python.private.pypi.whl_installer.wheel.host_interpreter_minor_version"
)


class DepsTest(unittest.TestCase):
    def test_simple(self):
        deps = wheel.Deps("foo", requires_dist=["bar"])

        got = deps.build()

        self.assertIsInstance(got, wheel.FrozenDeps)
        self.assertEqual(["bar"], got.deps)
        self.assertEqual({}, got.deps_select)

    def test_can_add_os_specific_deps(self):
        deps = wheel.Deps(
            "foo",
            requires_dist=[
                "bar",
                "an_osx_dep; sys_platform=='darwin'",
                "posix_dep; os_name=='posix'",
                "win_dep; os_name=='nt'",
            ],
            platforms={
                Platform(os=OS.linux, arch=Arch.x86_64),
                Platform(os=OS.osx, arch=Arch.x86_64),
                Platform(os=OS.osx, arch=Arch.aarch64),
                Platform(os=OS.windows, arch=Arch.x86_64),
            },
        )

        got = deps.build()

        self.assertEqual(["bar"], got.deps)
        self.assertEqual(
            {
                "@platforms//os:linux": ["posix_dep"],
                "@platforms//os:osx": ["an_osx_dep", "posix_dep"],
                "@platforms//os:windows": ["win_dep"],
            },
            got.deps_select,
        )

    def test_can_add_os_specific_deps_with_specific_python_version(self):
        deps = wheel.Deps(
            "foo",
            requires_dist=[
                "bar",
                "an_osx_dep; sys_platform=='darwin'",
                "posix_dep; os_name=='posix'",
                "win_dep; os_name=='nt'",
            ],
            platforms={
                Platform(os=OS.linux, arch=Arch.x86_64, minor_version=8),
                Platform(os=OS.osx, arch=Arch.x86_64, minor_version=8),
                Platform(os=OS.osx, arch=Arch.aarch64, minor_version=8),
                Platform(os=OS.windows, arch=Arch.x86_64, minor_version=8),
            },
        )

        got = deps.build()

        self.assertEqual(["bar"], got.deps)
        self.assertEqual(
            {
                "@platforms//os:linux": ["posix_dep"],
                "@platforms//os:osx": ["an_osx_dep", "posix_dep"],
                "@platforms//os:windows": ["win_dep"],
            },
            got.deps_select,
        )

    def test_deps_are_added_to_more_specialized_platforms(self):
        got = wheel.Deps(
            "foo",
            requires_dist=[
                "m1_dep; sys_platform=='darwin' and platform_machine=='arm64'",
                "mac_dep; sys_platform=='darwin'",
            ],
            platforms={
                Platform(os=OS.osx, arch=Arch.x86_64),
                Platform(os=OS.osx, arch=Arch.aarch64),
            },
        ).build()

        self.assertEqual(
            wheel.FrozenDeps(
                deps=[],
                deps_select={
                    "osx_aarch64": ["m1_dep", "mac_dep"],
                    "@platforms//os:osx": ["mac_dep"],
                },
            ),
            got,
        )

    def test_deps_from_more_specialized_platforms_are_propagated(self):
        got = wheel.Deps(
            "foo",
            requires_dist=[
                "a_mac_dep; sys_platform=='darwin'",
                "m1_dep; sys_platform=='darwin' and platform_machine=='arm64'",
            ],
            platforms={
                Platform(os=OS.osx, arch=Arch.x86_64),
                Platform(os=OS.osx, arch=Arch.aarch64),
            },
        ).build()

        self.assertEqual([], got.deps)
        self.assertEqual(
            {
                "osx_aarch64": ["a_mac_dep", "m1_dep"],
                "@platforms//os:osx": ["a_mac_dep"],
            },
            got.deps_select,
        )

    def test_non_platform_markers_are_added_to_common_deps(self):
        got = wheel.Deps(
            "foo",
            requires_dist=[
                "bar",
                "baz; implementation_name=='cpython'",
                "m1_dep; sys_platform=='darwin' and platform_machine=='arm64'",
            ],
            platforms={
                Platform(os=OS.linux, arch=Arch.x86_64),
                Platform(os=OS.osx, arch=Arch.x86_64),
                Platform(os=OS.osx, arch=Arch.aarch64),
                Platform(os=OS.windows, arch=Arch.x86_64),
            },
        ).build()

        self.assertEqual(["bar", "baz"], got.deps)
        self.assertEqual(
            {
                "osx_aarch64": ["m1_dep"],
            },
            got.deps_select,
        )

    def test_self_is_ignored(self):
        deps = wheel.Deps(
            "foo",
            requires_dist=[
                "bar",
                "req_dep; extra == 'requests'",
                "foo[requests]; extra == 'ssl'",
                "ssl_lib; extra == 'ssl'",
            ],
            extras={"ssl"},
        )

        got = deps.build()

        self.assertEqual(["bar", "req_dep", "ssl_lib"], got.deps)
        self.assertEqual({}, got.deps_select)

    def test_self_dependencies_can_come_in_any_order(self):
        deps = wheel.Deps(
            "foo",
            requires_dist=[
                "bar",
                "baz; extra == 'feat'",
                "foo[feat2]; extra == 'all'",
                "foo[feat]; extra == 'feat2'",
                "zdep; extra == 'all'",
            ],
            extras={"all"},
        )

        got = deps.build()

        self.assertEqual(["bar", "baz", "zdep"], got.deps)
        self.assertEqual({}, got.deps_select)

    def test_can_get_deps_based_on_specific_python_version(self):
        requires_dist = [
            "bar",
            "baz; python_version < '3.8'",
            "posix_dep; os_name=='posix' and python_version >= '3.8'",
        ]

        py38_deps = wheel.Deps(
            "foo",
            requires_dist=requires_dist,
            platforms=[
                Platform(os=OS.linux, arch=Arch.x86_64, minor_version=8),
            ],
        ).build()
        py37_deps = wheel.Deps(
            "foo",
            requires_dist=requires_dist,
            platforms=[
                Platform(os=OS.linux, arch=Arch.x86_64, minor_version=7),
            ],
        ).build()

        self.assertEqual(["bar", "baz"], py37_deps.deps)
        self.assertEqual({}, py37_deps.deps_select)
        self.assertEqual(["bar"], py38_deps.deps)
        self.assertEqual({"@platforms//os:linux": ["posix_dep"]}, py38_deps.deps_select)

    @mock.patch(_HOST_INTERPRETER_FN)
    def test_no_version_select_when_single_version(self, mock_host_interpreter_version):
        requires_dist = [
            "bar",
            "baz; python_version >= '3.8'",
            "posix_dep; os_name=='posix'",
            "posix_dep_with_version; os_name=='posix' and python_version >= '3.8'",
            "arch_dep; platform_machine=='x86_64' and python_version >= '3.8'",
        ]
        mock_host_interpreter_version.return_value = 7

        self.maxDiff = None

        deps = wheel.Deps(
            "foo",
            requires_dist=requires_dist,
            platforms=[
                Platform(os=os, arch=Arch.x86_64, minor_version=minor)
                for minor in [8]
                for os in [OS.linux, OS.windows]
            ],
        )
        got = deps.build()

        self.assertEqual(["bar", "baz"], got.deps)
        self.assertEqual(
            {
                "@platforms//os:linux": ["posix_dep", "posix_dep_with_version"],
                "linux_x86_64": ["arch_dep", "posix_dep", "posix_dep_with_version"],
                "windows_x86_64": ["arch_dep"],
            },
            got.deps_select,
        )

    @mock.patch(_HOST_INTERPRETER_FN)
    def test_can_get_version_select(self, mock_host_interpreter_version):
        requires_dist = [
            "bar",
            "baz; python_version < '3.8'",
            "baz_new; python_version >= '3.8'",
            "posix_dep; os_name=='posix'",
            "posix_dep_with_version; os_name=='posix' and python_version >= '3.8'",
            "arch_dep; platform_machine=='x86_64' and python_version < '3.8'",
        ]
        mock_host_interpreter_version.return_value = 7

        self.maxDiff = None

        deps = wheel.Deps(
            "foo",
            requires_dist=requires_dist,
            platforms=[
                Platform(os=os, arch=Arch.x86_64, minor_version=minor)
                for minor in [7, 8, 9]
                for os in [OS.linux, OS.windows]
            ],
        )
        got = deps.build()

        self.assertEqual(["bar"], got.deps)
        self.assertEqual(
            {
                "//conditions:default": ["baz"],
                "@//python/config_settings:is_python_3.7": ["baz"],
                "@//python/config_settings:is_python_3.8": ["baz_new"],
                "@//python/config_settings:is_python_3.9": ["baz_new"],
                "@platforms//os:linux": ["baz", "posix_dep"],
                "cp37_linux_x86_64": ["arch_dep", "baz", "posix_dep"],
                "cp37_windows_x86_64": ["arch_dep", "baz"],
                "cp37_linux_anyarch": ["baz", "posix_dep"],
                "cp38_linux_anyarch": [
                    "baz_new",
                    "posix_dep",
                    "posix_dep_with_version",
                ],
                "cp39_linux_anyarch": [
                    "baz_new",
                    "posix_dep",
                    "posix_dep_with_version",
                ],
                "linux_x86_64": ["arch_dep", "baz", "posix_dep"],
                "windows_x86_64": ["arch_dep", "baz"],
            },
            got.deps_select,
        )

    @mock.patch(_HOST_INTERPRETER_FN)
    def test_deps_spanning_all_target_py_versions_are_added_to_common(
        self, mock_host_version
    ):
        requires_dist = [
            "bar",
            "baz (<2,>=1.11) ; python_version < '3.8'",
            "baz (<2,>=1.14) ; python_version >= '3.8'",
        ]
        mock_host_version.return_value = 8

        deps = wheel.Deps(
            "foo",
            requires_dist=requires_dist,
            platforms=Platform.from_string(["cp37_*", "cp38_*", "cp39_*"]),
        )
        got = deps.build()

        self.assertEqual(["bar", "baz"], got.deps)
        self.assertEqual({}, got.deps_select)

    @mock.patch(_HOST_INTERPRETER_FN)
    def test_deps_are_not_duplicated(self, mock_host_version):
        mock_host_version.return_value = 7

        # See an example in
        # https://files.pythonhosted.org/packages/76/9e/db1c2d56c04b97981c06663384f45f28950a73d9acf840c4006d60d0a1ff/opencv_python-4.9.0.80-cp37-abi3-win32.whl.metadata
        requires_dist = [
            "bar >=0.1.0 ; python_version < '3.7'",
            "bar >=0.2.0 ; python_version >= '3.7'",
            "bar >=0.4.0 ; python_version >= '3.6' and platform_system == 'Linux' and platform_machine == 'aarch64'",
            "bar >=0.4.0 ; python_version >= '3.9'",
            "bar >=0.5.0 ; python_version <= '3.9' and platform_system == 'Darwin' and platform_machine == 'arm64'",
            "bar >=0.5.0 ; python_version >= '3.10' and platform_system == 'Darwin'",
            "bar >=0.5.0 ; python_version >= '3.10'",
            "bar >=0.6.0 ; python_version >= '3.11'",
        ]

        deps = wheel.Deps(
            "foo",
            requires_dist=requires_dist,
            platforms=Platform.from_string(["cp37_*", "cp310_*"]),
        )
        got = deps.build()

        self.assertEqual(["bar"], got.deps)
        self.assertEqual({}, got.deps_select)

    @mock.patch(_HOST_INTERPRETER_FN)
    def test_deps_are_not_duplicated_when_encountering_platform_dep_first(
        self, mock_host_version
    ):
        mock_host_version.return_value = 7

        # Note, that we are sorting the incoming `requires_dist` and we need to ensure that we are not getting any
        # issues even if the platform-specific line comes first.
        requires_dist = [
            "bar >=0.4.0 ; python_version >= '3.6' and platform_system == 'Linux' and platform_machine == 'aarch64'",
            "bar >=0.5.0 ; python_version >= '3.9'",
        ]

        deps = wheel.Deps(
            "foo",
            requires_dist=requires_dist,
            platforms=Platform.from_string(["cp37_*", "cp310_*"]),
        )
        got = deps.build()

        self.assertEqual(["bar"], got.deps)
        self.assertEqual({}, got.deps_select)


if __name__ == "__main__":
    unittest.main()
