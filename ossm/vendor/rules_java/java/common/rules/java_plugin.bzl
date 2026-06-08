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
Definition of java_plugin rule.
"""

load(":java_library.bzl", "JAVA_LIBRARY_ATTRS")
load(":rule_util.bzl", "merge_attrs")

visibility(["//java/..."])

JAVA_PLUGIN_ATTRS = merge_attrs(
    JAVA_LIBRARY_ATTRS,
    {
        "generates_api": attr.bool(doc = """
This attribute marks annotation processors that generate API code.
<p>If a rule uses an API-generating annotation processor, other rules
depending on it can refer to the generated code only if their
compilation actions are scheduled after the generating rule. This
attribute instructs Bazel to introduce scheduling constraints when
--java_header_compilation is enabled.
<p><em class="harmful">WARNING: This attribute affects build
performance, use it only if necessary.</em></p>
        """),
        "processor_class": attr.string(doc = """
The processor class is the fully qualified type of the class that the Java compiler should
use as entry point to the annotation processor. If not specified, this rule will not
contribute an annotation processor to the Java compiler's annotation processing, but its
runtime classpath will still be included on the compiler's annotation processor path. (This
is primarily intended for use by
<a href="https://errorprone.info/docs/plugins">Error Prone plugins</a>, which are loaded
from the annotation processor path using
<a href="https://docs.oracle.com/javase/8/docs/api/java/util/ServiceLoader.html">
java.util.ServiceLoader</a>.)
       """),
        # buildifier: disable=attr-licenses
        "output_licenses": attr.license() if hasattr(attr, "license") else attr.string_list(),
    },
    remove_attrs = ["runtime_deps", "exports", "exported_plugins"],
)
