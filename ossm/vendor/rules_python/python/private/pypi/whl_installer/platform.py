# Copyright 2024 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Utility class to inspect an extracted wheel directory"""

import platform
import sys
from dataclasses import dataclass
from enum import Enum
from typing import Any, Dict, Iterator, List, Optional, Union


class OS(Enum):
    linux = 1
    osx = 2
    windows = 3
    darwin = osx
    win32 = windows

    @classmethod
    def interpreter(cls) -> "OS":
        "Return the interpreter operating system."
        return cls[sys.platform.lower()]

    def __str__(self) -> str:
        return self.name.lower()


class Arch(Enum):
    x86_64 = 1
    x86_32 = 2
    aarch64 = 3
    ppc = 4
    s390x = 5
    arm = 6
    amd64 = x86_64
    arm64 = aarch64
    i386 = x86_32
    i686 = x86_32
    x86 = x86_32
    ppc64le = ppc

    @classmethod
    def interpreter(cls) -> "Arch":
        "Return the currently running interpreter architecture."
        # FIXME @aignas 2023-12-13: Hermetic toolchain on Windows 3.11.6
        # is returning an empty string here, so lets default to x86_64
        return cls[platform.machine().lower() or "x86_64"]

    def __str__(self) -> str:
        return self.name.lower()


def _as_int(value: Optional[Union[OS, Arch]]) -> int:
    """Convert one of the enums above to an int for easier sorting algorithms.

    Args:
        value: The value of an enum or None.

    Returns:
        -1 if we get None, otherwise, the numeric value of the given enum.
    """
    if value is None:
        return -1

    return int(value.value)


def host_interpreter_minor_version() -> int:
    return sys.version_info.minor


@dataclass(frozen=True)
class Platform:
    os: Optional[OS] = None
    arch: Optional[Arch] = None
    minor_version: Optional[int] = None

    @classmethod
    def all(
        cls,
        want_os: Optional[OS] = None,
        minor_version: Optional[int] = None,
    ) -> List["Platform"]:
        return sorted(
            [
                cls(os=os, arch=arch, minor_version=minor_version)
                for os in OS
                for arch in Arch
                if not want_os or want_os == os
            ]
        )

    @classmethod
    def host(cls) -> List["Platform"]:
        """Use the Python interpreter to detect the platform.

        We extract `os` from sys.platform and `arch` from platform.machine

        Returns:
            A list of parsed values which makes the signature the same as
            `Platform.all` and `Platform.from_string`.
        """
        return [
            Platform(
                os=OS.interpreter(),
                arch=Arch.interpreter(),
                minor_version=host_interpreter_minor_version(),
            )
        ]

    def all_specializations(self) -> Iterator["Platform"]:
        """Return the platform itself and all its unambiguous specializations.

        For more info about specializations see
        https://bazel.build/docs/configurable-attributes
        """
        yield self
        if self.arch is None:
            for arch in Arch:
                yield Platform(os=self.os, arch=arch, minor_version=self.minor_version)
        if self.os is None:
            for os in OS:
                yield Platform(os=os, arch=self.arch, minor_version=self.minor_version)
        if self.arch is None and self.os is None:
            for os in OS:
                for arch in Arch:
                    yield Platform(os=os, arch=arch, minor_version=self.minor_version)

    def __lt__(self, other: Any) -> bool:
        """Add a comparison method, so that `sorted` returns the most specialized platforms first."""
        if not isinstance(other, Platform) or other is None:
            raise ValueError(f"cannot compare {other} with Platform")

        self_arch, self_os = _as_int(self.arch), _as_int(self.os)
        other_arch, other_os = _as_int(other.arch), _as_int(other.os)

        if self_os == other_os:
            return self_arch < other_arch
        else:
            return self_os < other_os

    def __str__(self) -> str:
        if self.minor_version is None:
            if self.os is None and self.arch is None:
                return "//conditions:default"

            if self.arch is None:
                return f"@platforms//os:{self.os}"
            else:
                return f"{self.os}_{self.arch}"

        if self.arch is None and self.os is None:
            return f"@//python/config_settings:is_python_3.{self.minor_version}"

        if self.arch is None:
            return f"cp3{self.minor_version}_{self.os}_anyarch"

        if self.os is None:
            return f"cp3{self.minor_version}_anyos_{self.arch}"

        return f"cp3{self.minor_version}_{self.os}_{self.arch}"

    @classmethod
    def from_string(cls, platform: Union[str, List[str]]) -> List["Platform"]:
        """Parse a string and return a list of platforms"""
        platform = [platform] if isinstance(platform, str) else list(platform)
        ret = set()
        for p in platform:
            if p == "host":
                ret.update(cls.host())
                continue

            abi, _, tail = p.partition("_")
            if not abi.startswith("cp"):
                # The first item is not an abi
                tail = p
                abi = ""
            os, _, arch = tail.partition("_")
            arch = arch or "*"

            minor_version = int(abi[len("cp3") :]) if abi else None

            if arch != "*":
                ret.add(
                    cls(
                        os=OS[os] if os != "*" else None,
                        arch=Arch[arch],
                        minor_version=minor_version,
                    )
                )

            else:
                ret.update(
                    cls.all(
                        want_os=OS[os] if os != "*" else None,
                        minor_version=minor_version,
                    )
                )

        return sorted(ret)

    # NOTE @aignas 2023-12-05: below is the minimum number of accessors that are defined in
    # https://peps.python.org/pep-0496/ to make rules_python generate dependencies.
    #
    # WARNING: It may not work in cases where the python implementation is different between
    # different platforms.

    # derived from OS
    @property
    def os_name(self) -> str:
        if self.os == OS.linux or self.os == OS.osx:
            return "posix"
        elif self.os == OS.windows:
            return "nt"
        else:
            return ""

    @property
    def sys_platform(self) -> str:
        if self.os == OS.linux:
            return "linux"
        elif self.os == OS.osx:
            return "darwin"
        elif self.os == OS.windows:
            return "win32"
        else:
            return ""

    @property
    def platform_system(self) -> str:
        if self.os == OS.linux:
            return "Linux"
        elif self.os == OS.osx:
            return "Darwin"
        elif self.os == OS.windows:
            return "Windows"
        else:
            return ""

    # derived from OS and Arch
    @property
    def platform_machine(self) -> str:
        """Guess the target 'platform_machine' marker.

        NOTE @aignas 2023-12-05: this may not work on really new systems, like
        Windows if they define the platform markers in a different way.
        """
        if self.arch == Arch.x86_64:
            return "x86_64"
        elif self.arch == Arch.x86_32 and self.os != OS.osx:
            return "i386"
        elif self.arch == Arch.x86_32:
            return ""
        elif self.arch == Arch.aarch64 and self.os == OS.linux:
            return "aarch64"
        elif self.arch == Arch.aarch64:
            # Assuming that OSX and Windows use this one since the precedent is set here:
            # https://github.com/cgohlke/win_arm64-wheels
            return "arm64"
        elif self.os != OS.linux:
            return ""
        elif self.arch == Arch.ppc64le:
            return "ppc64le"
        elif self.arch == Arch.s390x:
            return "s390x"
        else:
            return ""

    def env_markers(self, extra: str) -> Dict[str, str]:
        # If it is None, use the host version
        minor_version = self.minor_version or host_interpreter_minor_version()

        return {
            "extra": extra,
            "os_name": self.os_name,
            "sys_platform": self.sys_platform,
            "platform_machine": self.platform_machine,
            "platform_system": self.platform_system,
            "platform_release": "",  # unset
            "platform_version": "",  # unset
            "python_version": f"3.{minor_version}",
            # FIXME @aignas 2024-01-14: is putting zero last a good idea? Maybe we should
            # use `20` or something else to avoid having weird issues where the full version is used for
            # matching and the author decides to only support 3.y.5 upwards.
            "implementation_version": f"3.{minor_version}.0",
            "python_full_version": f"3.{minor_version}.0",
            # we assume that the following are the same as the interpreter used to setup the deps:
            # "implementation_name": "cpython"
            # "platform_python_implementation: "CPython",
        }
