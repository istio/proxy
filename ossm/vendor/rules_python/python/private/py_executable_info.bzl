"""Implementation of PyExecutableInfo provider."""

PyExecutableInfo = provider(
    doc = """
Information about an executable.

This provider is for executable-specific information (e.g. tests and binaries).

:::{versionadded} 0.36.0
:::
""",
    fields = {
        "app_runfiles": """
:type: runfiles

The runfiles for the executable's "user" dependencies. These are things in e.g.
`deps` (or similar), but doesn't include "external" or "implicit" pieces,
e.g. the Python runtime itself. It's roughly akin to the files a traditional
venv would have installed into it.

:::{seealso}
{obj}`PyRuntimeInfo` for the Python runtime files. The {obj}`py_binary` et al
rules provide it directly so that the runtime the binary original chose
can be accessed.
:::

:::{versionadded} 1.9.0
:::
""",
        "build_data_file": """
:type: None | File

A symlink to build_data.txt if stamping is enabled, otherwise None.
""",
        "interpreter_args": """
:type: list[str]

Args that should be passed to the interpreter before regular args
(e.g. `-X whatever`).

:::{versionadded} 1.9.0
:::
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

:::{seealso}

The {obj}`stage2_bootstrap` attribute, which bootstraps an executable to run
the user main file.
:::
""",
        "runfiles_without_exe": """
:type: runfiles

The runfiles the program needs, but without the original executable,
files only added to support the original executable, or files specific to the
original program.
""",
        "stage2_bootstrap": """
:type: File | None

The Bazel-executable-level entry point to the program, which handles Bazel-specific
setup before running the file in {obj}`main`. May be None if a two-stage bootstrap
implementation isn't being used.

:::{versionadded} 1.9.0
:::
""",
        "venv_python_exe": """
:type: File | None

The `bin/python3` file within the venv this binary uses. May be None if venv
mode is not enabled.

:::{versionadded} 1.9.0
:::
""",
    },
)
