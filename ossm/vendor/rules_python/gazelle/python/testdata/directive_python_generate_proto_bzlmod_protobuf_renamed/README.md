# Directive: `python_generate_proto`

This test case asserts that the `# gazelle:python_generate_proto` directive
correctly reads the name of the protobuf repository when bzlmod is being used,
but the repository is renamed.

[gh-2994]: https://github.com/bazel-contrib/rules_python/issues/2994
