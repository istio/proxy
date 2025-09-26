# Getting a REPL or Interactive Shell

`rules_python` provides a REPL to help with debugging and developing. The goal of
the REPL is to present an environment identical to what a {bzl:obj}`py_binary` creates
for your code.

## Usage

Start the REPL with the following command:
```console
$ bazel run @rules_python//python/bin:repl
Python 3.11.11 (main, Mar 17 2025, 21:02:09) [Clang 20.1.0 ] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>>
```

Settings like `//python/config_settings:python_version` will influence the exact
behaviour.
```console
$ bazel run @rules_python//python/bin:repl --@rules_python//python/config_settings:python_version=3.13
Python 3.13.2 (main, Mar 17 2025, 21:02:54) [Clang 20.1.0 ] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>>
```

See [//python/config_settings](api/rules_python/python/config_settings/index)
and [Environment Variables](environment-variables) for more settings.

## Importing Python targets

The `//python/bin:repl_dep` command line flag gives the REPL access to a target
that provides the {bzl:obj}`PyInfo` provider.

```console
$ bazel run @rules_python//python/bin:repl --@rules_python//python/bin:repl_dep=@rules_python//tools:wheelmaker
Python 3.11.11 (main, Mar 17 2025, 21:02:09) [Clang 20.1.0 ] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>> import tools.wheelmaker
>>>
```

## Customizing the shell

By default, the `//python/bin:repl` target will invoke the shell from the `code`
module. It's possible to switch to another shell by writing a custom "stub" and
pointing the target at the necessary dependencies.

### IPython Example

For an IPython shell, create a file as follows.

```python
import IPython
IPython.start_ipython()
```

Assuming the file is called `ipython_stub.py` and the `pip.parse` hub's name is
`my_deps`, set this up in the .bazelrc file:
```
# Allow the REPL stub to import ipython. In this case, @my_deps is the hub name
# of the pip.parse() call.
build --@rules_python//python/bin:repl_stub_dep=@my_deps//ipython

# Point the REPL at the stub created above.
build --@rules_python//python/bin:repl_stub=//path/to:ipython_stub.py
```
