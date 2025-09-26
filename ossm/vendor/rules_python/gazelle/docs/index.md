# Gazelle Plugin

[Gazelle][gazelle] is a build file generator for Bazel projects. It can
create new `BUILD` or `BUILD.bazel` files for a project that
follows language conventions and update existing build files to include new
sources, dependencies, and options.

[gazelle]: https://github.com/bazel-contrib/bazel-gazelle

Bazel may run Gazelle using the Gazelle rule, or Gazelle may be installed and run
as a command line tool.

The {gh-path}`gazelle` directory contains a plugin for Gazelle
that generates `BUILD` files content for Python code. When Gazelle is
run as a command line tool with this plugin, it embeds a Python interpreter
resolved during the plugin build. The behavior of the plugin is slightly
different with different version of the interpreter as the Python
`stdlib` changes with every minor version release. Distributors of Gazelle
binaries should, therefore, build a Gazelle binary for each OS+CPU
architecture+Minor Python version combination they are targeting.

:::{note}
These instructions are for when you use [bzlmod][bzlmod]. Please refer to
older documentation that includes instructions on how to use Gazelle
without using bzlmod as your dependency manager.
:::

[bzlmod]: https://bazel.build/external/module

Gazelle is non-destructive. It will try to leave your edits to `BUILD`
files alone, only making updates to `py_*` targets. However it **will
remove** dependencies that appear to be unused, so it's a good idea to check
in your work before running Gazelle so you can easily revert any changes it made.

The `rules_python` extension assumes some conventions about your Python code.
These are noted in the subsequent documents, and might require changes to your
existing code.

Note that the `gazelle` program has multiple commands. At present, only
the `update` command (the default) does anything for Python code.


```{toctree}
:maxdepth: 1
installation_and_usage
directives
annotations
development
```
