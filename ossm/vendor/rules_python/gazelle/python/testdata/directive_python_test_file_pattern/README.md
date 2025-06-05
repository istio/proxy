# Directive: `python_test_file_pattern`

This test case asserts that the `# gazelle:python_test_file_pattern` directive
works as intended.

It consists of 6 cases:

1.  When not set, both `*_test.py` and `test_*.py` files are mapped to the `py_test`
    rule.
2.  When set to a single value `*_test.py`, `test_*.py` files are mapped to the
    `py_library` rule.
3.  When set to a single value `test_*.py`, `*_test.py` files are mapped to the
    `py_library` rule (ie: the inverse of case 2, but also with "file" generation
    mode).
4.  Arbitrary `glob` patterns are supported.
5.  Multiple `glob` patterns are supported and that patterns don't technically
    need to end in `.py` if they end in a wildcard (eg: we won't make a `py_test`
    target for the extensionless file `test_foo`).
6.  Sub-packages can override the directive's value.
