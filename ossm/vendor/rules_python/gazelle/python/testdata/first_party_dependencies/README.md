# First-party dependencies

There are 2 different scenarios that the extension needs to handle:

1. Import statements that match sub-directory names.
2. Import statements that don't match sub-directory names and need a hint from
   the user via directives.

This test case asserts that the generated targets cover both scenarios.

With the hint we need to check if it's a .py file or a directory with `__init__.py` file.
