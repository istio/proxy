import importlib.resources
from xml.etree import ElementTree

from absl.testing import absltest, parameterized

from sphinxdocs.tests import sphinx_stardoc


class SphinxOutputTest(parameterized.TestCase):
    def setUp(self):
        super().setUp()
        self._docs = {}
        self._xmls = {}

    def assert_xref(self, doc, *, text, href):
        match = self._doc_element(doc).find(f".//*[.='{text}']")
        if not match:
            self.fail(f"No element found with {text=}")
        actual = match.attrib.get("href", "<UNSET>")
        self.assertEqual(
            href,
            actual,
            msg=f"Unexpected href for {text=}: "
            + ElementTree.tostring(match).decode("utf8"),
        )

    def _read_doc(self, doc):
        doc += ".html"
        if doc not in self._docs:
            self._docs[doc] = (
                importlib.resources.files(sphinx_stardoc)
                .joinpath("docs/_build/html")
                .joinpath(doc)
                .read_text()
            )
        return self._docs[doc]

    def _doc_element(self, doc):
        xml = self._read_doc(doc)
        if doc not in self._xmls:
            self._xmls[doc] = ElementTree.fromstring(xml)
        return self._xmls[doc]

    @parameterized.named_parameters(
        # fmt: off
        ("short_func", "myfunc", "function.html#myfunc"),
        ("short_func_arg", "myfunc.arg1", "function.html#myfunc.arg1"),
        ("short_rule", "my_rule", "rule.html#my_rule"),
        ("short_rule_attr", "my_rule.ra1", "rule.html#my_rule.ra1"),
        ("short_provider", "LangInfo", "provider.html#LangInfo"),
        ("short_tag_class", "myext.mytag", "module_extension.html#myext.mytag"),
        ("full_norepo_func", "//lang:function.bzl%myfunc", "function.html#myfunc"),
        ("full_norepo_func_arg", "//lang:function.bzl%myfunc.arg1", "function.html#myfunc.arg1"),
        ("full_norepo_rule", "//lang:rule.bzl%my_rule", "rule.html#my_rule"),
        ("full_norepo_rule_attr", "//lang:rule.bzl%my_rule.ra1", "rule.html#my_rule.ra1"),
        ("full_norepo_provider", "//lang:provider.bzl%LangInfo", "provider.html#LangInfo"),
        ("full_norepo_aspect", "//lang:aspect.bzl%myaspect", "aspect.html#myaspect"),
        ("full_norepo_target", "//lang:relativetarget", "target.html#relativetarget"),
        ("full_repo_func", "@testrepo//lang:function.bzl%myfunc", "function.html#myfunc"),
        ("full_repo_func_arg", "@testrepo//lang:function.bzl%myfunc.arg1", "function.html#myfunc.arg1"),
        ("full_repo_rule", "@testrepo//lang:rule.bzl%my_rule", "rule.html#my_rule"),
        ("full_repo_rule_attr", "@testrepo//lang:rule.bzl%my_rule.ra1", "rule.html#my_rule.ra1"),
        ("full_repo_provider", "@testrepo//lang:provider.bzl%LangInfo", "provider.html#LangInfo"),
        ("full_repo_aspect", "@testrepo//lang:aspect.bzl%myaspect", "aspect.html#myaspect"),
        ("full_repo_target", "@testrepo//lang:relativetarget", "target.html#relativetarget"),
        # fmt: on
    )
    def test_xrefs(self, text, href):
        self.assert_xref("xrefs", text=text, href=href)


if __name__ == "__main__":
    absltest.main()
