# With third-party requirements

This test case asserts that 
* a `py_library` is generated with dependencies
extracted from its sources and a `py_binary` is generated embeding the
`py_library` and inherits its dependencies, without specifying the `deps` again.
* when a third-party library and a module in the same package having the same name, the one in the same package takes precedence.
