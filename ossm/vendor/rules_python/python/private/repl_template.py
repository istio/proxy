import os
import runpy
import sys
from pathlib import Path

from python.runfiles import runfiles

# runfiles.py will reject paths which aren't normalized, which can happen when the REPL rules are
# used from a remote module.
STUB_PATH = os.path.normpath("%stub_path%")


def start_repl():
    if sys.stdin.isatty():
        # Print the banner similar to how python does it on startup when running interactively.
        cprt = 'Type "help", "copyright", "credits" or "license" for more information.'
        sys.stderr.write("Python %s on %s\n%s\n" % (sys.version, sys.platform, cprt))

    # If there's a PYTHONSTARTUP script, we need to capture the new variables
    # that it defines.
    new_globals = {}

    # Simulate Python's behavior when a valid startup script is defined by the
    # PYTHONSTARTUP variable. If this file path fails to load, print the error
    # and revert to the default behavior.
    #
    # See upstream for more information:
    # https://docs.python.org/3/using/cmdline.html#envvar-PYTHONSTARTUP
    if startup_file := os.getenv("PYTHONSTARTUP"):
        try:
            source_code = Path(startup_file).read_text()
        except Exception as error:
            print(f"{type(error).__name__}: {error}")
        else:
            compiled_code = compile(source_code, filename=startup_file, mode="exec")
            eval(compiled_code, new_globals)

    bazel_runfiles = runfiles.Create()
    runpy.run_path(
        bazel_runfiles.Rlocation(STUB_PATH),
        init_globals=new_globals,
        run_name="__main__",
    )


if __name__ == "__main__":
    start_repl()
