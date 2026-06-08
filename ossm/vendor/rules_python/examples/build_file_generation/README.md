# Build file generation with Gazelle

This example shows a project that has Gazelle setup with the rules_python
extension, so that targets like `py_library` and `py_binary` can be
automatically created just by running

```sh
bazel run //:requirements.update
bazel run //:gazelle_python_manifest.update
bazel run //:gazelle
```

As a demo, try creating a `__main__.py` file in this directory, then
re-run that gazelle command. You'll see that a `py_binary` target
is created in the `BUILD` file.

Or, try importing the `requests` library in `__init__.py`.
You'll see that `deps = ["@pip//pypi__requests"]` is automatically
added to the `py_library` target in the `BUILD` file.

For more information on the behavior of the rules_python gazelle extension,
see the README.md file in the /gazelle folder.
