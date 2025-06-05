# Copyright 2023 The Bazel Authors. All rights reserved.
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
"""PyInfo testing subject."""

load("@rules_testing//lib:truth.bzl", "subjects")

def py_info_subject(info, *, meta):
    """Creates a new `PyInfoSubject` for a PyInfo provider instance.

    Method: PyInfoSubject.new

    Args:
        info: The PyInfo object
        meta: ExpectMeta object.

    Returns:
        A `PyInfoSubject` struct
    """

    # buildifier: disable=uninitialized
    public = struct(
        # go/keep-sorted start
        direct_original_sources = lambda *a, **k: _py_info_subject_direct_original_sources(self, *a, **k),
        direct_pyc_files = lambda *a, **k: _py_info_subject_direct_pyc_files(self, *a, **k),
        direct_pyi_files = lambda *a, **k: _py_info_subject_direct_pyi_files(self, *a, **k),
        has_py2_only_sources = lambda *a, **k: _py_info_subject_has_py2_only_sources(self, *a, **k),
        has_py3_only_sources = lambda *a, **k: _py_info_subject_has_py3_only_sources(self, *a, **k),
        imports = lambda *a, **k: _py_info_subject_imports(self, *a, **k),
        transitive_original_sources = lambda *a, **k: _py_info_subject_transitive_original_sources(self, *a, **k),
        transitive_pyc_files = lambda *a, **k: _py_info_subject_transitive_pyc_files(self, *a, **k),
        transitive_pyi_files = lambda *a, **k: _py_info_subject_transitive_pyi_files(self, *a, **k),
        transitive_sources = lambda *a, **k: _py_info_subject_transitive_sources(self, *a, **k),
        uses_shared_libraries = lambda *a, **k: _py_info_subject_uses_shared_libraries(self, *a, **k),
        # go/keep-sorted end
    )
    self = struct(
        actual = info,
        meta = meta,
    )
    return public

def _py_info_subject_direct_original_sources(self):
    """Returns a `DepsetFileSubject` for the `direct_original_sources` attribute.
    """
    return subjects.depset_file(
        self.actual.direct_original_sources,
        meta = self.meta.derive("direct_original_sources()"),
    )

def _py_info_subject_direct_pyc_files(self):
    """Returns a `DepsetFileSubject` for the `direct_pyc_files` attribute.

    Method: PyInfoSubject.direct_pyc_files
    """
    return subjects.depset_file(
        self.actual.direct_pyc_files,
        meta = self.meta.derive("direct_pyc_files()"),
    )

def _py_info_subject_direct_pyi_files(self):
    """Returns a `DepsetFileSubject` for the `direct_pyi_files` attribute.
    """
    return subjects.depset_file(
        self.actual.direct_pyi_files,
        meta = self.meta.derive("direct_pyi_files()"),
    )

def _py_info_subject_has_py2_only_sources(self):
    """Returns a `BoolSubject` for the `has_py2_only_sources` attribute.

    Method: PyInfoSubject.has_py2_only_sources
    """
    return subjects.bool(
        self.actual.has_py2_only_sources,
        meta = self.meta.derive("has_py2_only_sources()"),
    )

def _py_info_subject_has_py3_only_sources(self):
    """Returns a `BoolSubject` for the `has_py3_only_sources` attribute.

    Method: PyInfoSubject.has_py3_only_sources
    """
    return subjects.bool(
        self.actual.has_py3_only_sources,
        meta = self.meta.derive("has_py3_only_sources()"),
    )

def _py_info_subject_imports(self):
    """Returns a `CollectionSubject` for the `imports` attribute.

    Method: PyInfoSubject.imports
    """
    return subjects.collection(
        self.actual.imports.to_list(),
        meta = self.meta.derive("imports()"),
    )

def _py_info_subject_transitive_original_sources(self):
    """Returns a `DepsetFileSubject` for the `transitive_original_sources` attribute.

    Method: PyInfoSubject.transitive_original_sources
    """
    return subjects.depset_file(
        self.actual.transitive_original_sources,
        meta = self.meta.derive("transitive_original_sources()"),
    )

def _py_info_subject_transitive_pyc_files(self):
    """Returns a `DepsetFileSubject` for the `transitive_pyc_files` attribute.

    Method: PyInfoSubject.transitive_pyc_files
    """
    return subjects.depset_file(
        self.actual.transitive_pyc_files,
        meta = self.meta.derive("transitive_pyc_files()"),
    )

def _py_info_subject_transitive_pyi_files(self):
    """Returns a `DepsetFileSubject` for the `transitive_pyi_files` attribute.
    """
    return subjects.depset_file(
        self.actual.transitive_pyi_files,
        meta = self.meta.derive("transitive_pyi_files()"),
    )

def _py_info_subject_transitive_sources(self):
    """Returns a `DepsetFileSubject` for the `transitive_sources` attribute.

    Method: PyInfoSubject.transitive_sources
    """
    return subjects.depset_file(
        self.actual.transitive_sources,
        meta = self.meta.derive("transitive_sources()"),
    )

def _py_info_subject_uses_shared_libraries(self):
    """Returns a `BoolSubject` for the `uses_shared_libraries` attribute.

    Method: PyInfoSubject.uses_shared_libraries
    """
    return subjects.bool(
        self.actual.uses_shared_libraries,
        meta = self.meta.derive("uses_shared_libraries()"),
    )
