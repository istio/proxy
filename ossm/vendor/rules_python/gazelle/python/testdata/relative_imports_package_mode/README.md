# Resolve deps for relative imports

This test case verifies that the generated targets correctly handle relative imports in
Python. Specifically, when the Python generation mode is set to "package," it ensures
that relative import statements such as from .foo import X are properly resolved to
their corresponding modules.
