# Binary without entrypoint

This test case asserts that when there is no __main__.py, a py_binary is generated per file main module, and that this
py_binary is instead of (not in addition to) any py_library target.
