"""Implementation of PyExecutableInfo provider."""

PyExecutableInfo = provider(
    doc = """
Information about an executable.

This provider is for executable-specific information (e.g. tests and binaries).

:::{versionadded} 0.36.0
:::
""",
    fields = {
        "build_data_file": """
:type: None | File

A symlink to build_data.txt if stamping is enabled, otherwise None.
""",
        "interpreter_path": """
:type: None | str

Path to the Python interpreter to use for running the executable itself (not the
bootstrap script). Either an absolute path (which means it is
platform-specific), or a runfiles-relative path (which means the interpreter
should be within `runtime_files`)
""",
        "main": """
:type: File

The user-level entry point file. Usually a `.py` file, but may also be `.pyc`
file if precompiling is enabled.
""",
        "runfiles_without_exe": """
:type: runfiles

The runfiles the program needs, but without the original executable,
files only added to support the original executable, or files specific to the
original program.
""",
    },
)
