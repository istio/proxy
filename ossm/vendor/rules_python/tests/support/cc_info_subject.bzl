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
"""CcInfo testing subject."""

load("@rules_testing//lib:truth.bzl", "subjects")

def cc_info_subject(info, *, meta):
    """Creates a new `CcInfoSubject` for a CcInfo provider instance.

    Args:
        info: The CcInfo object.
        meta: ExpectMeta object.

    Returns:
        A `CcInfoSubject` struct.
    """

    # buildifier: disable=uninitialized
    public = struct(
        # go/keep-sorted start
        actual = info,
        compilation_context = lambda *a, **k: _cc_info_subject_compilation_context(self, *a, **k),
        linking_context = lambda *a, **k: _cc_info_subject_linking_context(self, *a, **k),
        # go/keep-sorted end
    )
    self = struct(
        actual = info,
        meta = meta,
    )
    return public

def _cc_info_subject_compilation_context(self):
    """Returns the CcInfo.compilation_context as a subject.

    Args:
        self: implicitly added.

    Returns:
        [`CompilationContext`] instance.
    """
    return _compilation_context_subject_new(
        self.actual.compilation_context,
        meta = self.meta.derive("compilation_context()"),
    )

def _cc_info_subject_linking_context(self):
    """Returns the CcInfo.linking_context as a subject.

    Args:
        self: implicitly added.

    Returns:
        [`LinkingContextSubject`] instance.
    """
    return _linking_context_subject_new(
        self.actual.linking_context,
        meta = self.meta.derive("linking_context()"),
    )

def _compilation_context_subject_new(info, *, meta):
    """Creates a CompilationContextSubject.

    Args:
        info: ([`CompilationContext`]) object instance.
        meta: rules_testing `ExpectMeta` instance.

    Returns:
        [`CompilationContextSubject`] object.
    """

    # buildifier: disable=uninitialized
    public = struct(
        # go/keep-sorted start
        direct_headers = lambda *a, **k: _compilation_context_subject_direct_headers(self, *a, **k),
        direct_public_headers = lambda *a, **k: _compilation_context_subject_direct_public_headers(self, *a, **k),
        system_includes = lambda *a, **k: _compilation_context_subject_system_includes(self, *a, **k),
        # go/keep-sorted end
    )
    self = struct(
        actual = info,
        meta = meta,
    )
    return public

def _compilation_context_subject_direct_headers(self):
    """Returns the direct headers as a subjecct.

    Args:
        self: implicitly added

    Returns:
        [`CollectionSubject`] of `File` objects of the direct headers.
    """
    return subjects.collection(
        self.actual.direct_headers,
        meta = self.meta.derive("direct_headers()"),
        container_name = "direct_headers",
        element_plural_name = "header files",
    )

def _compilation_context_subject_direct_public_headers(self):
    """Returns the direct public headers as a subjecct.

    Args:
        self: implicitly added

    Returns:
        [`CollectionSubject`] of `File` objects of the direct headers.
    """
    return subjects.collection(
        self.actual.direct_public_headers,
        meta = self.meta.derive("direct_public_headers()"),
        container_name = "direct_public_headers",
        element_plural_name = "public header files",
    )

def _compilation_context_subject_system_includes(self):
    """Returns the system include directories as a subject.

    NOTE: The system includes are the `cc_library.includes` attribute.

    Args:
        self: implicitly added

    Returns:
        [`CollectionSubject`] of [`str`]
    """
    return subjects.collection(
        self.actual.system_includes.to_list(),
        meta = self.meta.derive("includes()"),
        container_name = "includes",
        element_plural_name = "include paths",
    )

def _linking_context_subject_new(info, meta):
    """Creates a LinkingContextSubject.

    Args:
        info: ([`LinkingContext`]) object instance.
        meta: rules_testing `ExpectMeta` instance.

    Returns:
        [`LinkingContextSubject`] object.
    """

    # buildifier: disable=uninitialized
    public = struct(
        # go/keep-sorted start
        linker_inputs = lambda *a, **k: _linking_context_subject_linker_inputs(self, *a, **k),
        # go/keep-sorted end
    )
    self = struct(
        actual = info,
        meta = meta,
    )
    return public

def _linking_context_subject_linker_inputs(self):
    """Returns the linker inputs.

    Args:
        self: implicitly added

    Returns:
        [`CollectionSubject`] of the linker inputs.
    """
    return subjects.collection(
        self.actual.linker_inputs.to_list(),
        meta = self.meta.derive("linker_inputs()"),
        container_name = "linker_inputs",
        element_plural_name = "linker input values",
    )
