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
"""DefaultInfo testing subject."""

# TODO: Load this through truth.bzl#subjects when made available
# https://github.com/bazelbuild/rules_testing/issues/54
load("@rules_testing//lib/private:runfiles_subject.bzl", "RunfilesSubject")  # buildifier: disable=bzl-visibility

# TODO: Use rules_testing's DefaultInfoSubject once it's available
# https://github.com/bazelbuild/rules_testing/issues/52
def default_info_subject(info, *, meta):
    # buildifier: disable=uninitialized
    public = struct(
        runfiles = lambda *a, **k: _default_info_subject_runfiles(self, *a, **k),
    )
    self = struct(actual = info, meta = meta)
    return public

def _default_info_subject_runfiles(self):
    return RunfilesSubject.new(
        self.actual.default_runfiles,
        meta = self.meta.derive("runfiles()"),
    )
