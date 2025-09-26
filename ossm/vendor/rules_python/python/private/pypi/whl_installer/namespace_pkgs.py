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

"""Utility functions to discover python package types"""
import os
import textwrap
from pathlib import Path  # supported in >= 3.4
from typing import List, Optional, Set


def implicit_namespace_packages(
    directory: str, ignored_dirnames: Optional[List[str]] = None
) -> Set[Path]:
    """Discovers namespace packages implemented using the 'native namespace packages' method.

    AKA 'implicit namespace packages', which has been supported since Python 3.3.
    See: https://packaging.python.org/guides/packaging-namespace-packages/#native-namespace-packages

    Args:
        directory: The root directory to recursively find packages in.
        ignored_dirnames: A list of directories to exclude from the search

    Returns:
        The set of directories found under root to be packages using the native namespace method.
    """
    namespace_pkg_dirs: Set[Path] = set()
    standard_pkg_dirs: Set[Path] = set()
    directory_path = Path(directory)
    ignored_dirname_paths: List[Path] = [Path(p) for p in ignored_dirnames or ()]
    # Traverse bottom-up because a directory can be a namespace pkg because its child contains module files.
    for dirpath, dirnames, filenames in map(
        lambda t: (Path(t[0]), *t[1:]), os.walk(directory_path, topdown=False)
    ):
        if "__init__.py" in filenames:
            standard_pkg_dirs.add(dirpath)
            continue
        elif ignored_dirname_paths:
            is_ignored_dir = dirpath in ignored_dirname_paths
            child_of_ignored_dir = any(
                d in dirpath.parents for d in ignored_dirname_paths
            )
            if is_ignored_dir or child_of_ignored_dir:
                continue

        dir_includes_py_modules = _includes_python_modules(filenames)
        parent_of_namespace_pkg = any(
            Path(dirpath, d) in namespace_pkg_dirs for d in dirnames
        )
        parent_of_standard_pkg = any(
            Path(dirpath, d) in standard_pkg_dirs for d in dirnames
        )
        parent_of_pkg = parent_of_namespace_pkg or parent_of_standard_pkg
        if (
            (dir_includes_py_modules or parent_of_pkg)
            and
            # The root of the directory should never be an implicit namespace
            dirpath != directory_path
        ):
            namespace_pkg_dirs.add(dirpath)
    return namespace_pkg_dirs


def add_pkgutil_style_namespace_pkg_init(dir_path: Path) -> None:
    """Adds 'pkgutil-style namespace packages' init file to the given directory

    See: https://packaging.python.org/guides/packaging-namespace-packages/#pkgutil-style-namespace-packages

    Args:
        dir_path: The directory to create an __init__.py for.

    Raises:
        ValueError: If the directory already contains an __init__.py file
    """
    ns_pkg_init_filepath = os.path.join(dir_path, "__init__.py")

    if os.path.isfile(ns_pkg_init_filepath):
        raise ValueError("%s already contains an __init__.py file." % dir_path)

    with open(ns_pkg_init_filepath, "w") as ns_pkg_init_f:
        # See https://packaging.python.org/guides/packaging-namespace-packages/#pkgutil-style-namespace-packages
        ns_pkg_init_f.write(
            textwrap.dedent(
                """\
                # __path__ manipulation added by bazel-contrib/rules_python to support namespace pkgs.
                __path__ = __import__('pkgutil').extend_path(__path__, __name__)
                """
            )
        )


def _includes_python_modules(files: List[str]) -> bool:
    """
    In order to only transform directories that Python actually considers namespace pkgs
    we need to detect if a directory includes Python modules.

    Which files are loadable as modules is extension based, and the particular set of extensions
    varies by platform.

    See:
    1. https://github.com/python/cpython/blob/7d9d25dbedfffce61fc76bc7ccbfa9ae901bf56f/Lib/importlib/machinery.py#L19
    2. PEP 420 -- Implicit Namespace Packages, Specification - https://www.python.org/dev/peps/pep-0420/#specification
    3. dynload_shlib.c and dynload_win.c in python/cpython.
    """
    module_suffixes = {
        ".py",  # Source modules
        ".pyc",  # Compiled bytecode modules
        ".so",  # Unix extension modules
        ".pyd",  # https://docs.python.org/3/faq/windows.html#is-a-pyd-file-the-same-as-a-dll
    }
    return any(Path(f).suffix in module_suffixes for f in files)
