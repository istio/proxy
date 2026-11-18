# Images containing Python applications

Users are typically migrating from [py3_image](https://github.com/bazelbuild/rules_docker#py3_image)
in rules_docker.
(There is also an older `py_image` rule that's meant for Python 2, which is end-of-life and not considered here).

## Examples

- https://github.com/aspect-build/bazel-examples/tree/main/oci_python_image: shows how the image can be composed of three layers: interpreter, `site-packages`, and application.
