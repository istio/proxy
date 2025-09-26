# Binary without entrypoint

This test case asserts that when there is no __main__.py, a py_binary is generated per main module, unless a main
module main collides with existing target name.
