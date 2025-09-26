# Sibling imports

This test case asserts that imports from sibling modules are resolved correctly
when the `python_resolve_sibling_imports` directive is enabled (default
behavior). It covers 3 different types of imports in `pkg/unit_test.py`:

- `import a` - resolves to the sibling `a.py` in the same package
- `import test_util` - resolves to the sibling `test_util.py` in the same
  package
- `from b import run` - resolves to the sibling `b.py` in the same package

When sibling imports are enabled, we allow them to be satisfied by sibling
modules (ie. modules in the same package).
