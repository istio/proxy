# First-party file and directory module dependencies

This test case asserts that a `py_library` is generated with the dependencies
pointing to the correct first-party target that contains a Python module file
that was imported directly instead of a directory containing `__init__.py`.

Also, it asserts that the directory with the `__init__.py` file is selected
instead of a module file with same. E.g. `foo/__init__.py` takes precedence over
`foo.py` when `import foo` exists.
