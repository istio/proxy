# Partial update with multiple per-file binaries

This test case asserts that when there are multiple binaries in a package, and no __main__.py, and the BUILD file already includes a py_binary for one of the files, a py_binary is generated for the other file.
