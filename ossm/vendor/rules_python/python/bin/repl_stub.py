"""Simulates the REPL that Python spawns when invoking the binary with no arguments.

The code module is responsible for the default shell.

The import and `ocde.interact()` call here his is equivalent to doing:

    $ python3 -m code
    Python 3.11.2 (main, Mar 13 2023, 12:18:29) [GCC 12.2.0] on linux
    Type "help", "copyright", "credits" or "license" for more information.
    (InteractiveConsole)
    >>>

The logic for PYTHONSTARTUP is handled in python/private/repl_template.py.
"""

# Capture the globals from PYTHONSTARTUP so we can pass them on to the console.
console_locals = globals().copy()

import code
import rlcompleter
import sys


class DynamicCompleter(rlcompleter.Completer):
    """
    A custom completer that dynamically updates its namespace to include new
    imports made within the interactive session.
    """

    def __init__(self, namespace):
        # Store a reference to the namespace, not a copy, so that changes to the namespace are
        # reflected.
        self.namespace = namespace

    def complete(self, text, state):
        # Update the completer's internal namespace with the current interactive session's locals
        # and globals.  This is the key to making new imports discoverable.
        rlcompleter.Completer.__init__(self, self.namespace)
        return super().complete(text, state)


if sys.stdin.isatty():
    # Use the default options.
    exitmsg = None
else:
    # On a non-interactive console, we want to suppress the >>> and the exit message.
    exitmsg = ""
    sys.ps1 = ""
    sys.ps2 = ""

# Set up tab completion.
try:
    import readline

    completer = DynamicCompleter(console_locals)
    readline.set_completer(completer.complete)

    # TODO(jpwoodbu): Use readline.backend instead of readline.__doc__ once we can depend on having
    # Python >=3.13.
    if "libedit" in readline.__doc__:  # type: ignore
        readline.parse_and_bind("bind ^I rl_complete")
    elif "GNU readline" in readline.__doc__:  # type: ignore
        readline.parse_and_bind("tab: complete")
    else:
        print(
            "Could not enable tab completion: "
            "unable to determine readline backend"
        )
except ImportError:
    print(
        "Could not enable tab completion: "
        "readline module not available on this platform"
    )

# We set the banner to an empty string because the repl_template.py file already prints the banner.
code.interact(local=console_locals, banner="", exitmsg=exitmsg)
