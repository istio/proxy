"""doc protobuf and grpc rules."""

load(":doc_docbook_compile.bzl", _doc_docbook_compile = "doc_docbook_compile")
load(":doc_html_compile.bzl", _doc_html_compile = "doc_html_compile")
load(":doc_json_compile.bzl", _doc_json_compile = "doc_json_compile")
load(":doc_markdown_compile.bzl", _doc_markdown_compile = "doc_markdown_compile")
load(":doc_template_compile.bzl", _doc_template_compile = "doc_template_compile")

# Export doc rules
doc_docbook_compile = _doc_docbook_compile
doc_html_compile = _doc_html_compile
doc_json_compile = _doc_json_compile
doc_markdown_compile = _doc_markdown_compile
doc_template_compile = _doc_template_compile
