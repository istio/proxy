# Bzlmod build file generation example

This example demostrates how to use `rules_python` and gazelle with `bzlmod` enabled.
[Bzlmod](https://bazel.build/external/overview#bzlmod), the new external dependency 
subsystem, does not directly work with repo definitions. Instead, it builds a dependency 
graph from modules, runs extensions on top of the graph, and defines repos accordingly.

Gazelle is setup with the `rules_python`
extension, so that targets like `py_library` and `py_binary` can be
automatically created just by running:

```sh
$ bazel run //:gazelle update
```

The are other targets that allow you to update the gazelle dependency management
when you update the requirements.in file.  See:

```bash
bazel run //:gazelle_python_manifest.update
```

For more information on the behavior of the `rules_python` gazelle extension,
see the [README.md](../../gazelle/README.md) file in the /gazelle folder.

This example uses a `MODULE.bazel` file that configures the bzlmod dependency
management. See comments in the `MODULE.bazel` and `BUILD.bazel` files for more
information.
