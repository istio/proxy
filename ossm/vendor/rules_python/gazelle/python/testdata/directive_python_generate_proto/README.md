# Directive: `python_generate_proto`

This test case asserts that the `# gazelle:python_generate_proto` directive
correctly:

1.  Uses the default value when `python_generate_proto` is not set.
2.  Generates (or not) `py_proto_library` when `python_generate_proto` is set, based on whether a proto is present.

[gh-2994]: https://github.com/bazel-contrib/rules_python/issues/2994
