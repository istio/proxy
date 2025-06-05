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

import io
import re

from absl.testing import absltest
from google.protobuf import text_format
from stardoc.proto import stardoc_output_pb2

from sphinxdocs.private import proto_to_markdown

_EVERYTHING_MODULE = """\
module_docstring: "MODULE_DOC_STRING"
file: "@repo//pkg:foo.bzl"

rule_info: {
  rule_name: "rule_1"
  doc_string: "RULE_1_DOC_STRING"
  attribute: {
    name: "rule_1_attr_1",
    doc_string: "RULE_1_ATTR_1_DOC_STRING"
    type: STRING
    default_value: "RULE_1_ATTR_1_DEFAULT_VALUE"
  }
}
provider_info: {
  provider_name: "ProviderAlpha"
  doc_string: "PROVIDER_ALPHA_DOC_STRING"
  field_info: {
    name: "ProviderAlpha_field_a"
    doc_string: "PROVIDER_ALPHA_FIELD_A_DOC_STRING"
  }
}
func_info: {
  function_name: "function_1"
  doc_string: "FUNCTION_1_DOC_STRING"
  parameter: {
    name: "function_1_param_a"
    doc_string: "FUNCTION_1_PARAM_A_DOC_STRING"
    default_value: "FUNCTION_1_PARAM_A_DEFAULT_VALUE"
  }
  return: {
    doc_string: "FUNCTION_1_RETURN_DOC_STRING"
  }
  deprecated: {
    doc_string: "FUNCTION_1_DEPRECATED_DOC_STRING"
  }
}
aspect_info: {
  aspect_name: "aspect_1"
  doc_string: "ASPECT_1_DOC_STRING"
  aspect_attribute: "aspect_1_aspect_attribute_a"
  attribute: {
    name: "aspect_1_attribute_a",
    doc_string: "ASPECT_1_ATTRIBUTE_A_DOC_STRING"
    type: INT
    default_value: "694638"
  }
}
module_extension_info: {
  extension_name: "bzlmod_ext"
  doc_string: "BZLMOD_EXT_DOC_STRING"
  tag_class: {
    tag_name: "bzlmod_ext_tag_a"
    doc_string: "BZLMOD_EXT_TAG_A_DOC_STRING"
    attribute: {
      name: "bzlmod_ext_tag_a_attribute_1",
      doc_string: "BZLMOD_EXT_TAG_A_ATTRIBUTE_1_DOC_STRING"
      type: STRING_LIST
      default_value: "[BZLMOD_EXT_TAG_A_ATTRIBUTE_1_DEFAULT_VALUE]"
    }
  }
  tag_class: {
    tag_name: "bzlmod_ext_tag_no_doc"
    attribute: {
      name: "bzlmod_ext_tag_a_attribute_2",
      type: STRING_LIST
      default_value: "[BZLMOD_EXT_TAG_A_ATTRIBUTE_2_DEFAULT_VALUE]"
    }
  }
}
repository_rule_info: {
  rule_name: "repository_rule",
  doc_string: "REPOSITORY_RULE_DOC_STRING"
  attribute: {
    name: "repository_rule_attribute_a",
    doc_string: "REPOSITORY_RULE_ATTRIBUTE_A_DOC_STRING"
    type: BOOLEAN
    default_value: "True"
  }
  environ: "ENV_VAR_A"
}
"""


class ProtoToMarkdownTest(absltest.TestCase):
    def setUp(self):
        super().setUp()
        self.stream = io.StringIO()

    def _render(self, module_text):
        renderer = proto_to_markdown._MySTRenderer(
            module=text_format.Parse(module_text, stardoc_output_pb2.ModuleInfo()),
            out_stream=self.stream,
            public_load_path="",
        )
        renderer.render()
        return self.stream.getvalue()

    def test_basic_rendering_everything(self):
        actual = self._render(_EVERYTHING_MODULE)

        self.assertIn("{bzl:currentfile} //pkg:foo.bzl", actual)
        self.assertRegex(actual, "# //pkg:foo.bzl")
        self.assertRegex(actual, "MODULE_DOC_STRING")

        self.assertRegex(actual, "{bzl:rule} rule_1.*")
        self.assertRegex(actual, "RULE_1_DOC_STRING")
        self.assertRegex(actual, "rule_1_attr_1")
        self.assertRegex(actual, "RULE_1_ATTR_1_DOC_STRING")
        self.assertRegex(actual, "RULE_1_ATTR_1_DEFAULT_VALUE")

        self.assertRegex(actual, "{bzl:provider} ProviderAlpha")
        self.assertRegex(actual, "PROVIDER_ALPHA_DOC_STRING")
        self.assertRegex(actual, "ProviderAlpha_field_a")
        self.assertRegex(actual, "PROVIDER_ALPHA_FIELD_A_DOC_STRING")

        self.assertRegex(actual, "{bzl:function} function_1")
        self.assertRegex(actual, "FUNCTION_1_DOC_STRING")
        self.assertRegex(actual, "function_1_param_a")
        self.assertRegex(actual, "FUNCTION_1_PARAM_A_DOC_STRING")
        self.assertRegex(actual, "FUNCTION_1_PARAM_A_DEFAULT_VALUE")
        self.assertRegex(actual, "FUNCTION_1_RETURN_DOC_STRING")
        self.assertRegex(actual, "FUNCTION_1_DEPRECATED_DOC_STRING")

        self.assertRegex(actual, "{bzl:aspect} aspect_1")
        self.assertRegex(actual, "ASPECT_1_DOC_STRING")
        self.assertRegex(actual, "aspect_1_aspect_attribute_a")
        self.assertRegex(actual, "aspect_1_attribute_a")
        self.assertRegex(actual, "ASPECT_1_ATTRIBUTE_A_DOC_STRING")
        self.assertRegex(actual, "694638")

        self.assertRegex(actual, "{bzl:module-extension} bzlmod_ext")
        self.assertRegex(actual, "BZLMOD_EXT_DOC_STRING")
        self.assertRegex(actual, "{bzl:tag-class} bzlmod_ext_tag_a")
        self.assertRegex(actual, "BZLMOD_EXT_TAG_A_DOC_STRING")
        self.assertRegex(actual, "bzlmod_ext_tag_a_attribute_1")
        self.assertRegex(actual, "BZLMOD_EXT_TAG_A_ATTRIBUTE_1_DOC_STRING")
        self.assertRegex(actual, "BZLMOD_EXT_TAG_A_ATTRIBUTE_1_DEFAULT_VALUE")
        self.assertRegex(actual, "{bzl:tag-class} bzlmod_ext_tag_no_doc")
        self.assertRegex(actual, "bzlmod_ext_tag_a_attribute_2")
        self.assertRegex(actual, "BZLMOD_EXT_TAG_A_ATTRIBUTE_2_DEFAULT_VALUE")

        self.assertRegex(actual, "{bzl:repo-rule} repository_rule")
        self.assertRegex(actual, "REPOSITORY_RULE_DOC_STRING")
        self.assertRegex(actual, "repository_rule_attribute_a")
        self.assertRegex(actual, "REPOSITORY_RULE_ATTRIBUTE_A_DOC_STRING")
        self.assertRegex(actual, "repository_rule_attribute_a.*=.*True")
        self.assertRegex(actual, "ENV_VAR_A")

    def test_render_signature(self):
        actual = self._render(
            """\
file: "@repo//pkg:foo.bzl"
func_info: {
  function_name: "func"
  parameter: {
    name: "param_with_default"
    default_value: "DEFAULT"
  }
  parameter: {
    name: "param_without_default"
  }
  parameter: {
    name: "param_with_function_default",
    default_value: "<function foo from //bar:baz.bzl>"
  }
  parameter: {
    name: "param_with_label_default",
    default_value: 'Label(*, "@repo//pkg:file.bzl")'
  }
  parameter: {
    name: "last_param"
  }
}
        """
        )
        self.assertIn("param_with_default=DEFAULT,", actual)
        self.assertIn("{default-value}`DEFAULT`", actual)
        self.assertIn(":arg param_with_default:", actual)
        self.assertIn("param_without_default,", actual)
        self.assertIn('{default-value}`"@repo//pkg:file.bzl"`', actual)
        self.assertIn("{default-value}`'<function foo from //bar:baz.bzl>'", actual)

    def test_render_typedefs(self):
        proto_text = """
file: "@repo//pkg:foo.bzl"
func_info: { function_name: "Zeta.TYPEDEF" }
func_info: { function_name: "Carl.TYPEDEF" }
func_info: { function_name: "Carl.ns.Alpha.TYPEDEF" }
func_info: { function_name: "Beta.TYPEDEF" }
func_info: { function_name: "Beta.Sub.TYPEDEF" }
"""
        actual = self._render(proto_text)
        self.assertIn("\n:::::::::::::{bzl:typedef} Beta\n", actual)
        self.assertIn("\n::::::::::::{bzl:typedef} Beta.Sub\n", actual)
        self.assertIn("\n:::::::::::::{bzl:typedef} Carl\n", actual)
        self.assertIn("\n::::::::::::{bzl:typedef} Carl.ns.Alpha\n", actual)
        self.assertIn("\n:::::::::::::{bzl:typedef} Zeta\n", actual)

    def test_render_func_no_doc_with_args(self):
        proto_text = """
file: "@repo//pkg:foo.bzl"
func_info: {
  function_name: "func"
  parameter: {
    name: "param"
    doc_string: "param_doc"
  }
}
"""
        actual = self._render(proto_text)
        expected = """
:::::::::::::{bzl:function} func(*param)

:arg param:
  param_doc

:::::::::::::
"""
        self.assertIn(expected, actual)

    def test_render_module_extension(self):
        proto_text = """
file: "@repo//pkg:foo.bzl"
module_extension_info: {
  extension_name: "bzlmod_ext"
  tag_class: {
    tag_name: "bzlmod_ext_tag_a"
    doc_string: "BZLMOD_EXT_TAG_A_DOC_STRING"
    attribute: {
      name: "attr1",
      doc_string: "attr1doc"
      type: STRING_LIST
    }
  }
}
"""
        actual = self._render(proto_text)
        expected = """
:::::{bzl:tag-class} bzlmod_ext_tag_a(attr1)

BZLMOD_EXT_TAG_A_DOC_STRING

:attr attr1:
  {type}`list[str]`
  attr1doc
  :::{bzl:attr-info} Info
  :::


:::::
::::::
"""
        self.assertIn(expected, actual)


if __name__ == "__main__":
    absltest.main()
