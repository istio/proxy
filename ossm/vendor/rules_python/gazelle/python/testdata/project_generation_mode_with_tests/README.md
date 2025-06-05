# Project generation mode with tests

Example using `gazelle:python_generation_mode project` in a project with tests, but no `__test__.py` entrypoint.

Note that, in this mode, the `py_test` rule will have no `main` set, which will fail to run with the standard
`py_test` rule. However, this can be used in conjunction with `gazelle:map_kind` to use some other implementation
of `py_test` that is able to handle this sitation (such as `rules_python_pytest`).