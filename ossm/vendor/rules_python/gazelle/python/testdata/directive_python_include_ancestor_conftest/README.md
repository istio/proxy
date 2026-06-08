# Directive: `python_include_ancestor_conftest`

This test case asserts that the `# gazelle:python_include_ancestor_conftest`
directive correctly includes or excludes ancestor `conftest` targets in
`py_test` target dependencies.

The test also asserts that the directive can be applied at any level and that
child levels will inherit the value:

+ The root level does not set the directive (it defaults to True).
+ The next level, `one/`, inherits that value.
+ The next level, `one/two/`, sets the directive to False; consequently the
  `py_test` target only includes the sibling `:conftest` target.
  + The `one/two/no_conftest/` directory does not contain a `conftest.py` file
    thereby asserting that we correctly do not include any `conftest` targets
    whatsoever.
+ The final level, `one/two/three/`, sets the directive back to True, meaning
  the `py_test` target includes a total of 4 `conftest` targets.
  + The `one/two/three/no_conftest/` directory does not contain a `conftest.py`
    file and thus asserts that the code includes _only_ ancestor `conftest`
    targets.

See [Issue #3595](https://github.com/bazel-contrib/rules_python/issues/3595).
