# Directive: `python_default_visibility`

This test case asserts that the `# gazelle:python_default_visibility` directive
correctly:

1.  Uses the default value when `python_default_visibility` is not set.
2.  Uses the correct default value when `python_root` is set and
    `python_default_visibility` is not set.
3.  Supports injecting `python_root`
4.  Supports multiple labels
5.  Setting the label to "NONE" removes all visibility attibutes.
6.  Setting the label to "DEFAULT" reverts to using the default.
7.  Adding `python_visibility` directive with `python_default_visibility NONE`
    only adds the items listed by `python_visibility`.
8.  Multiple `python_root` dirs [GH #1682][gh-1682] uses correct value when
    injecting `python_root`.
9.  Setting both `python_default_visibility` and `python_visibility` and how
    they interact with sub-packages.


[gh-1682]: https://github.com/bazel-contrib/rules_python/issues/1682
