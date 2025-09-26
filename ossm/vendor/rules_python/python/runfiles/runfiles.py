# Copyright 2018 The Bazel Authors. All rights reserved.
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

"""Runfiles lookup library for Bazel-built Python binaries and tests.

See @rules_python//python/runfiles/README.md for usage instructions.
"""
import inspect
import os
import posixpath
import sys
from typing import Dict, Optional, Tuple, Union


class _ManifestBased:
    """`Runfiles` strategy that parses a runfiles-manifest to look up runfiles."""

    def __init__(self, path: str) -> None:
        if not path:
            raise ValueError()
        if not isinstance(path, str):
            raise TypeError()
        self._path = path
        self._runfiles = _ManifestBased._LoadRunfiles(path)

    def RlocationChecked(self, path: str) -> Optional[str]:
        """Returns the runtime path of a runfile."""
        exact_match = self._runfiles.get(path)
        if exact_match:
            return exact_match
        # If path references a runfile that lies under a directory that
        # itself is a runfile, then only the directory is listed in the
        # manifest. Look up all prefixes of path in the manifest and append
        # the relative path from the prefix to the looked up path.
        prefix_end = len(path)
        while True:
            prefix_end = path.rfind("/", 0, prefix_end - 1)
            if prefix_end == -1:
                return None
            prefix_match = self._runfiles.get(path[0:prefix_end])
            if prefix_match:
                return prefix_match + "/" + path[prefix_end + 1 :]

    @staticmethod
    def _LoadRunfiles(path: str) -> Dict[str, str]:
        """Loads the runfiles manifest."""
        result = {}
        with open(path, "r", encoding="utf-8", newline="\n") as f:
            for line in f:
                line = line.rstrip("\n")
                if line.startswith(" "):
                    # In lines that start with a space, spaces, newlines, and backslashes are escaped as \s, \n, and \b in
                    # link and newlines and backslashes are escaped in target.
                    escaped_link, escaped_target = line[1:].split(" ", maxsplit=1)
                    link = (
                        escaped_link.replace(r"\s", " ")
                        .replace(r"\n", "\n")
                        .replace(r"\b", "\\")
                    )
                    target = escaped_target.replace(r"\n", "\n").replace(r"\b", "\\")
                else:
                    link, target = line.split(" ", maxsplit=1)

                if target:
                    result[link] = target
                else:
                    result[link] = link
        return result

    def _GetRunfilesDir(self) -> str:
        if self._path.endswith("/MANIFEST") or self._path.endswith("\\MANIFEST"):
            return self._path[: -len("/MANIFEST")]
        if self._path.endswith(".runfiles_manifest"):
            return self._path[: -len("_manifest")]
        return ""

    def EnvVars(self) -> Dict[str, str]:
        directory = self._GetRunfilesDir()
        return {
            "RUNFILES_MANIFEST_FILE": self._path,
            "RUNFILES_DIR": directory,
            # TODO(laszlocsomor): remove JAVA_RUNFILES once the Java launcher can
            # pick up RUNFILES_DIR.
            "JAVA_RUNFILES": directory,
        }


class _DirectoryBased:
    """`Runfiles` strategy that appends runfiles paths to the runfiles root."""

    def __init__(self, path: str) -> None:
        if not path:
            raise ValueError()
        if not isinstance(path, str):
            raise TypeError()
        self._runfiles_root = path

    def RlocationChecked(self, path: str) -> str:
        # Use posixpath instead of os.path, because Bazel only creates a runfiles
        # tree on Unix platforms, so `Create()` will only create a directory-based
        # runfiles strategy on those platforms.
        return posixpath.join(self._runfiles_root, path)

    def EnvVars(self) -> Dict[str, str]:
        return {
            "RUNFILES_DIR": self._runfiles_root,
            # TODO(laszlocsomor): remove JAVA_RUNFILES once the Java launcher can
            # pick up RUNFILES_DIR.
            "JAVA_RUNFILES": self._runfiles_root,
        }


class Runfiles:
    """Returns the runtime location of runfiles.

    Runfiles are data-dependencies of Bazel-built binaries and tests.
    """

    def __init__(self, strategy: Union[_ManifestBased, _DirectoryBased]) -> None:
        self._strategy = strategy
        self._python_runfiles_root = _FindPythonRunfilesRoot()
        self._repo_mapping = _ParseRepoMapping(
            strategy.RlocationChecked("_repo_mapping")
        )

    def Rlocation(self, path: str, source_repo: Optional[str] = None) -> Optional[str]:
        """Returns the runtime path of a runfile.

        Runfiles are data-dependencies of Bazel-built binaries and tests.

        The returned path may not be valid. The caller should check the path's
        validity and that the path exists.

        The function may return None. In that case the caller can be sure that the
        rule does not know about this data-dependency.

        Args:
          path: string; runfiles-root-relative path of the runfile
          source_repo: string; optional; the canonical name of the repository
            whose repository mapping should be used to resolve apparent to
            canonical repository names in `path`. If `None` (default), the
            repository mapping of the repository containing the caller of this
            method is used. Explicitly setting this parameter should only be
            necessary for libraries that want to wrap the runfiles library. Use
            `CurrentRepository` to obtain canonical repository names.
        Returns:
          the path to the runfile, which the caller should check for existence, or
          None if the method doesn't know about this runfile
        Raises:
          TypeError: if `path` is not a string
          ValueError: if `path` is None or empty, or it's absolute or not normalized
        """
        if not path:
            raise ValueError()
        if not isinstance(path, str):
            raise TypeError()
        if (
            path.startswith("../")
            or "/.." in path
            or path.startswith("./")
            or "/./" in path
            or path.endswith("/.")
            or "//" in path
        ):
            raise ValueError('path is not normalized: "%s"' % path)
        if path[0] == "\\":
            raise ValueError('path is absolute without a drive letter: "%s"' % path)
        if os.path.isabs(path):
            return path

        if source_repo is None and self._repo_mapping:
            # Look up runfiles using the repository mapping of the caller of the
            # current method. If the repo mapping is empty, determining this
            # name is not necessary.
            source_repo = self.CurrentRepository(frame=2)

        # Split off the first path component, which contains the repository
        # name (apparent or canonical).
        target_repo, _, remainder = path.partition("/")
        if not remainder or (source_repo, target_repo) not in self._repo_mapping:
            # One of the following is the case:
            # - not using Bzlmod, so the repository mapping is empty and
            #   apparent and canonical repository names are the same
            # - target_repo is already a canonical repository name and does not
            #   have to be mapped.
            # - path did not contain a slash and referred to a root symlink,
            #   which also should not be mapped.
            return self._strategy.RlocationChecked(path)

        assert (
            source_repo is not None
        ), "BUG: if the `source_repo` is None, we should never go past the `if` statement above"

        # target_repo is an apparent repository name. Look up the corresponding
        # canonical repository name with respect to the current repository,
        # identified by its canonical name.
        target_canonical = self._repo_mapping[(source_repo, target_repo)]
        return self._strategy.RlocationChecked(target_canonical + "/" + remainder)

    def EnvVars(self) -> Dict[str, str]:
        """Returns environment variables for subprocesses.

        The caller should set the returned key-value pairs in the environment of
        subprocesses in case those subprocesses are also Bazel-built binaries that
        need to use runfiles.

        Returns:
          {string: string}; a dict; keys are environment variable names, values are
          the values for these environment variables
        """
        return self._strategy.EnvVars()

    def CurrentRepository(self, frame: int = 1) -> str:
        """Returns the canonical name of the caller's Bazel repository.

        For example, this function returns '' (the empty string) when called
        from the main repository and a string of the form
        'rules_python~0.13.0` when called from code in the repository
        corresponding to the rules_python Bazel module.

        More information about the difference between canonical repository
        names and the `@repo` part of labels is available at:
        https://bazel.build/build/bzlmod#repository-names

        NOTE: This function inspects the callstack to determine where in the
        runfiles the caller is located to determine which repository it came
        from. This may fail or produce incorrect results depending on who the
        caller is, for example if it is not represented by a Python source
        file. Use the `frame` argument to control the stack lookup.

        Args:
            frame: int; the stack frame to return the repository name for.
            Defaults to 1, the caller of the CurrentRepository function.

        Returns:
            The canonical name of the Bazel repository containing the file
            containing the frame-th caller of this function

        Raises:
            ValueError: if the caller cannot be determined or the caller's file
            path is not contained in the Python runfiles tree
        """
        try:
            # pylint: disable-next=protected-access
            caller_path = inspect.getfile(sys._getframe(frame))
        except (TypeError, ValueError) as exc:
            raise ValueError("failed to determine caller's file path") from exc
        caller_runfiles_path = os.path.relpath(caller_path, self._python_runfiles_root)
        if caller_runfiles_path.startswith(".." + os.path.sep):
            # With Python 3.10 and earlier, sys.path contains the directory
            # of the script, which can result in a module being loaded from
            # outside the runfiles tree. In this case, assume that the module is
            # located in the main repository.
            # With Python 3.11 and higher, the Python launcher sets
            # PYTHONSAFEPATH, which prevents this behavior.
            # TODO: This doesn't cover the case of a script being run from an
            #       external repository, which could be heuristically detected
            #       by parsing the script's path.
            if (
                sys.version_info.minor <= 10
                and sys.path[0] != self._python_runfiles_root
            ):
                return ""
            raise ValueError(
                "{} does not lie under the runfiles root {}".format(
                    caller_path, self._python_runfiles_root
                )
            )

        caller_runfiles_directory = caller_runfiles_path[
            : caller_runfiles_path.find(os.path.sep)
        ]
        # With Bzlmod, the runfiles directory of the main repository is always
        # named "_main". Without Bzlmod, the value returned by this function is
        # never used, so we just assume Bzlmod is enabled.
        if caller_runfiles_directory == "_main":
            # The canonical name of the main repository (also known as the
            # workspace) is the empty string.
            return ""
        # For all other repositories, the name of the runfiles directory is the
        # canonical name.
        return caller_runfiles_directory

    # TODO: Update return type to Self when 3.11 is the min version
    # https://peps.python.org/pep-0673/
    @staticmethod
    def CreateManifestBased(manifest_path: str) -> "Runfiles":
        return Runfiles(_ManifestBased(manifest_path))

    # TODO: Update return type to Self when 3.11 is the min version
    # https://peps.python.org/pep-0673/
    @staticmethod
    def CreateDirectoryBased(runfiles_dir_path: str) -> "Runfiles":
        return Runfiles(_DirectoryBased(runfiles_dir_path))

    # TODO: Update return type to Self when 3.11 is the min version
    # https://peps.python.org/pep-0673/
    @staticmethod
    def Create(env: Optional[Dict[str, str]] = None) -> Optional["Runfiles"]:
        """Returns a new `Runfiles` instance.

        The returned object is either:
        - manifest-based, meaning it looks up runfile paths from a manifest file, or
        - directory-based, meaning it looks up runfile paths under a given directory
        path

        If `env` contains "RUNFILES_MANIFEST_FILE" with non-empty value, this method
        returns a manifest-based implementation. The object eagerly reads and caches
        the whole manifest file upon instantiation; this may be relevant for
        performance consideration.

        Otherwise, if `env` contains "RUNFILES_DIR" with non-empty value (checked in
        this priority order), this method returns a directory-based implementation.

        If neither cases apply, this method returns null.

        Args:
        env: {string: string}; optional; the map of environment variables. If None,
            this function uses the environment variable map of this process.
        Raises:
        IOError: if some IO error occurs.
        """
        env_map = os.environ if env is None else env
        manifest = env_map.get("RUNFILES_MANIFEST_FILE")
        if manifest:
            return CreateManifestBased(manifest)

        directory = env_map.get("RUNFILES_DIR")
        if directory:
            return CreateDirectoryBased(directory)

        return None


# Support legacy imports by defining a private symbol.
_Runfiles = Runfiles


def _FindPythonRunfilesRoot() -> str:
    """Finds the root of the Python runfiles tree."""
    root = __file__
    # Walk up our own runfiles path to the root of the runfiles tree from which
    # the current file is being run. This path coincides with what the Bazel
    # Python stub sets up as sys.path[0]. Since that entry can be changed at
    # runtime, we rederive it here.
    for _ in range("rules_python/python/runfiles/runfiles.py".count("/") + 1):
        root = os.path.dirname(root)
    return root


def _ParseRepoMapping(repo_mapping_path: Optional[str]) -> Dict[Tuple[str, str], str]:
    """Parses the repository mapping manifest."""
    # If the repository mapping file can't be found, that is not an error: We
    # might be running without Bzlmod enabled or there may not be any runfiles.
    # In this case, just apply an empty repo mapping.
    if not repo_mapping_path:
        return {}
    try:
        with open(repo_mapping_path, "r", encoding="utf-8", newline="\n") as f:
            content = f.read()
    except FileNotFoundError:
        return {}

    repo_mapping = {}
    for line in content.split("\n"):
        if not line:
            # Empty line following the last line break
            break
        current_canonical, target_local, target_canonical = line.split(",")
        repo_mapping[(current_canonical, target_local)] = target_canonical

    return repo_mapping


def CreateManifestBased(manifest_path: str) -> Runfiles:
    return Runfiles.CreateManifestBased(manifest_path)


def CreateDirectoryBased(runfiles_dir_path: str) -> Runfiles:
    return Runfiles.CreateDirectoryBased(runfiles_dir_path)


def Create(env: Optional[Dict[str, str]] = None) -> Optional[Runfiles]:
    return Runfiles.Create(env)
