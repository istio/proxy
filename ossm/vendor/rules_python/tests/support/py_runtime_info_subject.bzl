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
"""PyRuntimeInfo testing subject."""

load("@rules_testing//lib:truth.bzl", "subjects")

def py_runtime_info_subject(info, *, meta):
    """Creates a new `PyRuntimeInfoSubject` for a PyRuntimeInfo provider instance.

    Method: PyRuntimeInfoSubject.new

    Args:
        info: The PyRuntimeInfo object
        meta: ExpectMeta object.

    Returns:
        A `PyRuntimeInfoSubject` struct
    """

    # buildifier: disable=uninitialized
    public = struct(
        # go/keep-sorted start
        actual = info,
        bootstrap_template = lambda *a, **k: _py_runtime_info_subject_bootstrap_template(self, *a, **k),
        coverage_files = lambda *a, **k: _py_runtime_info_subject_coverage_files(self, *a, **k),
        coverage_tool = lambda *a, **k: _py_runtime_info_subject_coverage_tool(self, *a, **k),
        files = lambda *a, **k: _py_runtime_info_subject_files(self, *a, **k),
        interpreter = lambda *a, **k: _py_runtime_info_subject_interpreter(self, *a, **k),
        interpreter_path = lambda *a, **k: _py_runtime_info_subject_interpreter_path(self, *a, **k),
        interpreter_version_info = lambda *a, **k: _py_runtime_info_subject_interpreter_version_info(self, *a, **k),
        python_version = lambda *a, **k: _py_runtime_info_subject_python_version(self, *a, **k),
        stub_shebang = lambda *a, **k: _py_runtime_info_subject_stub_shebang(self, *a, **k),
        # go/keep-sorted end
    )
    self = struct(
        actual = info,
        meta = meta,
    )
    return public

def _py_runtime_info_subject_bootstrap_template(self):
    return subjects.file(
        self.actual.bootstrap_template,
        meta = self.meta.derive("bootstrap_template()"),
    )

def _py_runtime_info_subject_coverage_files(self):
    """Returns a `DepsetFileSubject` for the `coverage_files` attribute.

    Args:
        self: implicitly added.
    """
    return subjects.depset_file(
        self.actual.coverage_files,
        meta = self.meta.derive("coverage_files()"),
    )

def _py_runtime_info_subject_coverage_tool(self):
    return subjects.file(
        self.actual.coverage_tool,
        meta = self.meta.derive("coverage_tool()"),
    )

def _py_runtime_info_subject_files(self):
    return subjects.depset_file(
        self.actual.files,
        meta = self.meta.derive("files()"),
    )

def _py_runtime_info_subject_interpreter(self):
    return subjects.file(
        self.actual.interpreter,
        meta = self.meta.derive("interpreter()"),
    )

def _py_runtime_info_subject_interpreter_path(self):
    return subjects.str(
        self.actual.interpreter_path,
        meta = self.meta.derive("interpreter_path()"),
    )

def _py_runtime_info_subject_python_version(self):
    return subjects.str(
        self.actual.python_version,
        meta = self.meta.derive("python_version()"),
    )

def _py_runtime_info_subject_stub_shebang(self):
    return subjects.str(
        self.actual.stub_shebang,
        meta = self.meta.derive("stub_shebang()"),
    )

def _py_runtime_info_subject_interpreter_version_info(self):
    return subjects.struct(
        self.actual.interpreter_version_info,
        attrs = dict(
            major = subjects.int,
            minor = subjects.int,
            micro = subjects.int,
            releaselevel = subjects.str,
            serial = subjects.int,
        ),
        meta = self.meta.derive("interpreter_version_info()"),
    )
