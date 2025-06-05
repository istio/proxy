# Copyright 2024 The Bazel Authors. All rights reserved.
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
"""PyExecutableInfo testing subject."""

load("@rules_testing//lib:truth.bzl", "subjects")

def _py_executable_info_subject_new(info, *, meta):
    """Creates a new `PyExecutableInfoSubject` for a PyExecutableInfo provider instance.

    Method: PyExecutableInfoSubject.new

    Args:
        info: The PyExecutableInfo object
        meta: ExpectMeta object.

    Returns:
        A `PyExecutableInfoSubject` struct
    """

    # buildifier: disable=uninitialized
    public = struct(
        # go/keep-sorted start
        actual = info,
        interpreter_path = lambda *a, **k: _py_executable_info_subject_interpreter_path(self, *a, **k),
        main = lambda *a, **k: _py_executable_info_subject_main(self, *a, **k),
        runfiles_without_exe = lambda *a, **k: _py_executable_info_subject_runfiles_without_exe(self, *a, **k),
        # go/keep-sorted end
    )
    self = struct(
        actual = info,
        meta = meta,
    )
    return public

def _py_executable_info_subject_interpreter_path(self):
    """Returns a subject for `PyExecutableInfo.interpreter_path`."""
    return subjects.str(
        self.actual.interpreter_path,
        meta = self.meta.derive("interpreter_path()"),
    )

def _py_executable_info_subject_main(self):
    """Returns a subject for `PyExecutableInfo.main`."""
    return subjects.file(
        self.actual.main,
        meta = self.meta.derive("main()"),
    )

def _py_executable_info_subject_runfiles_without_exe(self):
    """Returns a subject for `PyExecutableInfo.runfiles_without_exe`."""
    return subjects.runfiles(
        self.actual.runfiles_without_exe,
        meta = self.meta.derive("runfiles_without_exe()"),
    )

# buildifier: disable=name-conventions
PyExecutableInfoSubject = struct(
    new = _py_executable_info_subject_new,
)
