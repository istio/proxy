# Copyright 2023 The Bazel Authors. All rights reserved.
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

import email
import re
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

import installer
from packaging.requirements import Requirement
from pip._vendor.packaging.utils import canonicalize_name

from python.private.pypi.whl_installer.platform import (
    Platform,
    host_interpreter_minor_version,
)


@dataclass(frozen=True)
class FrozenDeps:
    deps: List[str]
    deps_select: Dict[str, List[str]]


class Deps:
    """Deps is a dependency builder that has a build() method to return FrozenDeps."""

    def __init__(
        self,
        name: str,
        requires_dist: List[str],
        *,
        extras: Optional[Set[str]] = None,
        platforms: Optional[Set[Platform]] = None,
    ):
        """Create a new instance and parse the requires_dist

        Args:
            name (str): The name of the whl distribution
            requires_dist (list[Str]): The Requires-Dist from the METADATA of the whl
                distribution.
            extras (set[str], optional): The list of requested extras, defaults to None.
            platforms (set[Platform], optional): The list of target platforms, defaults to
                None. If the list of platforms has multiple `minor_version` values, it
                will change the code to generate the select statements using
                `@rules_python//python/config_settings:is_python_3.y` conditions.
        """
        self.name: str = Deps._normalize(name)
        self._platforms: Set[Platform] = platforms or set()
        self._target_versions = {p.minor_version for p in platforms or {}}
        self._default_minor_version = None
        if platforms and len(self._target_versions) > 2:
            # TODO @aignas 2024-06-23: enable this to be set via a CLI arg
            # for being more explicit.
            self._default_minor_version = host_interpreter_minor_version()

        if None in self._target_versions and len(self._target_versions) > 2:
            raise ValueError(
                f"all python versions need to be specified explicitly, got: {platforms}"
            )

        # Sort so that the dictionary order in the FrozenDeps is deterministic
        # without the final sort because Python retains insertion order. That way
        # the sorting by platform is limited within the Platform class itself and
        # the unit-tests for the Deps can be simpler.
        reqs = sorted(
            (Requirement(wheel_req) for wheel_req in requires_dist),
            key=lambda x: f"{x.name}:{sorted(x.extras)}",
        )

        want_extras = self._resolve_extras(reqs, extras)

        # Then add all of the requirements in order
        self._deps: Set[str] = set()
        self._select: Dict[Platform, Set[str]] = defaultdict(set)
        for req in reqs:
            self._add_req(req, want_extras)

    def _add(self, dep: str, platform: Optional[Platform]):
        dep = Deps._normalize(dep)

        # Self-edges are processed in _resolve_extras
        if dep == self.name:
            return

        if not platform:
            self._deps.add(dep)

            # If the dep is in the platform-specific list, remove it from the select.
            pop_keys = []
            for p, deps in self._select.items():
                if dep not in deps:
                    continue

                deps.remove(dep)
                if not deps:
                    pop_keys.append(p)

            for p in pop_keys:
                self._select.pop(p)
            return

        if dep in self._deps:
            # If the dep is already in the main dependency list, no need to add it in the
            # platform-specific dependency list.
            return

        # Add the platform-specific dep
        self._select[platform].add(dep)

        # Add the dep to specializations of the given platform if they
        # exist in the select statement.
        for p in platform.all_specializations():
            if p not in self._select:
                continue

            self._select[p].add(dep)

        if len(self._select[platform]) == 1:
            # We are adding a new item to the select and we need to ensure that
            # existing dependencies from less specialized platforms are propagated
            # to the newly added dependency set.
            for p, deps in self._select.items():
                # Check if the existing platform overlaps with the given platform
                if p == platform or platform not in p.all_specializations():
                    continue

                self._select[platform].update(self._select[p])

    def _maybe_add_common_dep(self, dep):
        if len(self._target_versions) < 2:
            return

        platforms = [Platform()] + [
            Platform(minor_version=v) for v in self._target_versions
        ]

        # If the dep is targeting all target python versions, lets add it to
        # the common dependency list to simplify the select statements.
        for p in platforms:
            if p not in self._select:
                return

            if dep not in self._select[p]:
                return

        # All of the python version-specific branches have the dep, so lets add
        # it to the common deps.
        self._deps.add(dep)
        for p in platforms:
            self._select[p].remove(dep)
            if not self._select[p]:
                self._select.pop(p)

    @staticmethod
    def _normalize(name: str) -> str:
        return re.sub(r"[-_.]+", "_", name).lower()

    def _resolve_extras(
        self, reqs: List[Requirement], extras: Optional[Set[str]]
    ) -> Set[str]:
        """Resolve extras which are due to depending on self[some_other_extra].

        Some packages may have cyclic dependencies resulting from extras being used, one example is
        `etils`, where we have one set of extras as aliases for other extras
        and we have an extra called 'all' that includes all other extras.

        Example: github.com/google/etils/blob/a0b71032095db14acf6b33516bca6d885fe09e35/pyproject.toml#L32.

        When the `requirements.txt` is generated by `pip-tools`, then it is likely that
        this step is not needed, but for other `requirements.txt` files this may be useful.

        NOTE @aignas 2023-12-08: the extra resolution is not platform dependent,
        but in order for it to become platform dependent we would have to have
        separate targets for each extra in extras.
        """

        # Resolve any extra extras due to self-edges, empty string means no
        # extras The empty string in the set is just a way to make the handling
        # of no extras and a single extra easier and having a set of {"", "foo"}
        # is equivalent to having {"foo"}.
        extras = extras or {""}

        self_reqs = []
        for req in reqs:
            if Deps._normalize(req.name) != self.name:
                continue

            if req.marker is None:
                # I am pretty sure we cannot reach this code as it does not
                # make sense to specify packages in this way, but since it is
                # easy to handle, lets do it.
                #
                # TODO @aignas 2023-12-08: add a test
                extras = extras | req.extras
            else:
                # process these in a separate loop
                self_reqs.append(req)

        # A double loop is not strictly optimal, but always correct without recursion
        for req in self_reqs:
            if any(req.marker.evaluate({"extra": extra}) for extra in extras):
                extras = extras | req.extras
            else:
                continue

            # Iterate through all packages to ensure that we include all of the extras from previously
            # visited packages.
            for req_ in self_reqs:
                if any(req_.marker.evaluate({"extra": extra}) for extra in extras):
                    extras = extras | req_.extras

        return extras

    def _add_req(self, req: Requirement, extras: Set[str]) -> None:
        if req.marker is None:
            self._add(req.name, None)
            return

        marker_str = str(req.marker)

        if not self._platforms:
            if any(req.marker.evaluate({"extra": extra}) for extra in extras):
                self._add(req.name, None)
            return

        # NOTE @aignas 2023-12-08: in order to have reasonable select statements
        # we do have to have some parsing of the markers, so it begs the question
        # if packaging should be reimplemented in Starlark to have the best solution
        # for now we will implement it in Python and see what the best parsing result
        # can be before making this decision.
        match_os = any(
            tag in marker_str
            for tag in [
                "os_name",
                "sys_platform",
                "platform_system",
            ]
        )
        match_arch = "platform_machine" in marker_str
        match_version = "version" in marker_str

        if not (match_os or match_arch or match_version):
            if any(req.marker.evaluate({"extra": extra}) for extra in extras):
                self._add(req.name, None)
            return

        for plat in self._platforms:
            if not any(
                req.marker.evaluate(plat.env_markers(extra)) for extra in extras
            ):
                continue

            if match_arch and self._default_minor_version:
                self._add(req.name, plat)
                if plat.minor_version == self._default_minor_version:
                    self._add(req.name, Platform(plat.os, plat.arch))
            elif match_arch:
                self._add(req.name, Platform(plat.os, plat.arch))
            elif match_os and self._default_minor_version:
                self._add(req.name, Platform(plat.os, minor_version=plat.minor_version))
                if plat.minor_version == self._default_minor_version:
                    self._add(req.name, Platform(plat.os))
            elif match_os:
                self._add(req.name, Platform(plat.os))
            elif match_version and self._default_minor_version:
                self._add(req.name, Platform(minor_version=plat.minor_version))
                if plat.minor_version == self._default_minor_version:
                    self._add(req.name, Platform())
            elif match_version:
                self._add(req.name, None)

        # Merge to common if possible after processing all platforms
        self._maybe_add_common_dep(req.name)

    def build(self) -> FrozenDeps:
        return FrozenDeps(
            deps=sorted(self._deps),
            deps_select={str(p): sorted(deps) for p, deps in self._select.items()},
        )


class Wheel:
    """Representation of the compressed .whl file"""

    def __init__(self, path: Path):
        self._path = path

    @property
    def path(self) -> Path:
        return self._path

    @property
    def name(self) -> str:
        # TODO Also available as installer.sources.WheelSource.distribution
        name = str(self.metadata["Name"])
        return canonicalize_name(name)

    @property
    def metadata(self) -> email.message.Message:
        with installer.sources.WheelFile.open(self.path) as wheel_source:
            metadata_contents = wheel_source.read_dist_info("METADATA")
            metadata = installer.utils.parse_metadata_file(metadata_contents)
        return metadata

    @property
    def version(self) -> str:
        # TODO Also available as installer.sources.WheelSource.version
        return str(self.metadata["Version"])

    def entry_points(self) -> Dict[str, Tuple[str, str]]:
        """Returns the entrypoints defined in the current wheel

        See https://packaging.python.org/specifications/entry-points/ for more info

        Returns:
            Dict[str, Tuple[str, str]]: A mapping of the entry point's name to it's module and attribute
        """
        with installer.sources.WheelFile.open(self.path) as wheel_source:
            if "entry_points.txt" not in wheel_source.dist_info_filenames:
                return dict()

            entry_points_mapping = dict()
            entry_points_contents = wheel_source.read_dist_info("entry_points.txt")
            entry_points = installer.utils.parse_entrypoints(entry_points_contents)
            for script, module, attribute, script_section in entry_points:
                if script_section == "console":
                    entry_points_mapping[script] = (module, attribute)

            return entry_points_mapping

    def dependencies(
        self,
        extras_requested: Set[str] = None,
        platforms: Optional[Set[Platform]] = None,
    ) -> FrozenDeps:
        return Deps(
            self.name,
            extras=extras_requested,
            platforms=platforms,
            requires_dist=self.metadata.get_all("Requires-Dist", []),
        ).build()

    def unzip(self, directory: str) -> None:
        installation_schemes = {
            "purelib": "/site-packages",
            "platlib": "/site-packages",
            "headers": "/include",
            "scripts": "/bin",
            "data": "/data",
        }
        destination = installer.destinations.SchemeDictionaryDestination(
            installation_schemes,
            # TODO Should entry_point scripts also be handled by installer rather than custom code?
            interpreter="/dev/null",
            script_kind="posix",
            destdir=directory,
            bytecode_optimization_levels=[],
        )

        with installer.sources.WheelFile.open(self.path) as wheel_source:
            installer.install(
                source=wheel_source,
                destination=destination,
                additional_metadata={
                    "INSTALLER": b"https://github.com/bazelbuild/rules_python",
                },
            )
