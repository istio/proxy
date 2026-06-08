# Directive: python_generate_pyi_srcs

Test that the `python_generate_pyi_srcs` directive will add `pyi_srcs` to
generated targets and that it can be toggled on/off on a per-package basis.

The root of the test case asserts that the default generation mode (package)
will compile multiple .pyi files into a single py_* target.

The `per_file` directory asserts that the `file` generation mode will attach
a single .pyi file to a given target.

Lastly, the `per_file/turn_off` directory asserts that we can turn off the
directive for subpackages. It continues with per-file generation mode.
