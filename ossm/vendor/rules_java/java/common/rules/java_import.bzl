# Copyright 2021 The Bazel Authors. All rights reserved.
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

"""
Definition of java_import rule.
"""

load("//java/common:java_info.bzl", "JavaInfo")
load("//java/common:java_semantics.bzl", "semantics")

visibility(["//java/..."])

_ALLOWED_RULES_IN_DEPS_FOR_JAVA_IMPORT = [
    "java_library",
    "java_import",
    "cc_library",
    "cc_binary",
]

# buildifier: disable=attr-licenses
JAVA_IMPORT_ATTRS = {
    "data": attr.label_list(
        allow_files = True,
        flags = ["SKIP_CONSTRAINTS_OVERRIDE"],
        doc = """
The list of files needed by this rule at runtime.
        """,
    ),
    "deps": attr.label_list(
        providers = [JavaInfo],
        allow_rules = _ALLOWED_RULES_IN_DEPS_FOR_JAVA_IMPORT,
        doc = """
The list of other libraries to be linked in to the target.
See <a href="${link java_library.deps}">java_library.deps</a>.
        """,
    ),
    "exports": attr.label_list(
        providers = [JavaInfo],
        allow_rules = _ALLOWED_RULES_IN_DEPS_FOR_JAVA_IMPORT,
        doc = """
Targets to make available to users of this rule.
See <a href="${link java_library.exports}">java_library.exports</a>.
        """,
    ),
    "runtime_deps": attr.label_list(
        allow_files = [".jar"],
        allow_rules = _ALLOWED_RULES_IN_DEPS_FOR_JAVA_IMPORT,
        providers = [[CcInfo], [JavaInfo]],
        flags = ["SKIP_ANALYSIS_TIME_FILETYPE_CHECK"],
        doc = """
Libraries to make available to the final binary or test at runtime only.
See <a href="${link java_library.runtime_deps}">java_library.runtime_deps</a>.
        """,
    ),
    # JavaImportBazeRule attr
    "jars": attr.label_list(
        allow_files = [".jar"],
        mandatory = True,
        doc = """
The list of JAR files provided to Java targets that depend on this target.
        """,
    ),
    "srcjar": attr.label(
        allow_single_file = [".srcjar", ".jar"],
        flags = ["DIRECT_COMPILE_TIME_INPUT"],
        doc = """
A JAR file that contains source code for the compiled JAR files.
        """,
    ),
    "neverlink": attr.bool(
        default = False,
        doc = """
Only use this library for compilation and not at runtime.
Useful if the library will be provided by the runtime environment
during execution. Examples of libraries like this are IDE APIs
for IDE plug-ins or <code>tools.jar</code> for anything running on
a standard JDK.
        """,
    ),
    "constraints": attr.string_list(
        doc = """
Extra constraints imposed on this rule as a Java library.
        """,
    ),
    # ProguardLibraryRule attr
    "proguard_specs": attr.label_list(
        allow_files = True,
        doc = """
Files to be used as Proguard specification.
These will describe the set of specifications to be used by Proguard. If specified,
they will be added to any <code>android_binary</code> target depending on this library.

The files included here must only have idempotent rules, namely -dontnote, -dontwarn,
assumenosideeffects, and rules that start with -keep. Other options can only appear in
<code>android_binary</code>'s proguard_specs, to ensure non-tautological merges.
        """,
    ),
    # Additional attrs
    "add_exports": attr.string_list(
        doc = """
Allow this library to access the given <code>module</code> or <code>package</code>.
<p>
This corresponds to the javac and JVM --add-exports= flags.
        """,
    ),
    "add_opens": attr.string_list(
        doc = """
Allow this library to reflectively access the given <code>module</code> or
<code>package</code>.
<p>
This corresponds to the javac and JVM --add-opens= flags.
        """,
    ),
    "licenses": attr.license() if hasattr(attr, "license") else attr.string_list(),
    "_java_toolchain_type": attr.label(default = semantics.JAVA_TOOLCHAIN_TYPE),
}
