# Starlark docgen

Using the `sphinx_stardoc` rule, API documentation can be generated from bzl
source code. This rule requires both MyST-based markdown and the `sphinx_bzl`
Sphinx extension are enabled. This allows source code to use Markdown and
Sphinx syntax to create rich documentation with cross references, types, and
more.


## Configuring Sphinx

While the `sphinx_stardoc` rule doesn't require Sphinx itself, the source
it generates requires some additional Sphinx plugins and config settings.

When defining the `sphinx_build_binary` target, also depend on:
* `@rules_python//sphinxdocs/src/sphinx_bzl:sphinx_bzl`
* `myst_parser` (e.g. `@pypi//myst_parser`)
* `typing_extensions` (e.g. `@pypi//myst_parser`)

```
sphinx_build_binary(
    name = "sphinx-build",
    deps = [
        "@rules_python//sphinxdocs/src/sphinx_bzl",
        "@pypi//myst_parser",
        "@pypi//typing_extensions",
        ...
    ]
)
```

In `conf.py`, enable the `sphinx_bzl` extension, `myst_parser` extension,
and the `colon_fence` MyST extension.

```
extensions = [
    "myst_parser",
    "sphinx_bzl.bzl",
]

myst_enable_extensions = [
    "colon_fence",
]
```

## Generating docs from bzl files

To convert the bzl code to Sphinx doc sources, `sphinx_stardocs` is the primary
rule to do so. It takes a list of `bzl_library` targets or files and generates docs for
each. When a `bzl_library` target is passed, the `bzl_library.srcs` value can only
have a single file.

Example:

```
sphinx_stardocs(
    name = "my_docs",
    srcs = [
      ":binary_bzl",
      ":library_bzl",
    ]
)

bzl_library(
   name = "binary_bzl",
   srcs = ["binary.bzl"],
   deps = ...
)

bzl_library(
   name = "library_bzl",
   srcs = ["library.bzl"],
   deps = ...
)
```

## User-defined types

While Starlark doesn't have user-defined types as a first-class concept, it's
still possible to create such objects using `struct` and lambdas. For the
purposes of documentation, they can be documented by creating a module-level
`struct` with matching fields *and* also a field named `TYPEDEF`. When the
`sphinx_stardoc` rule sees a struct with a `TYPEDEF` field, it generates doc
using the {rst:directive}`bzl:typedef` directive and puts all the struct's fields
within the typedef. The net result is the rendered docs look similar to how
a class would be documented in other programming languages.

For example, a the Starlark implemenation of a `Square` object with a `area()`
method would look like:

```

def _Square_typedef():
    """A square with fixed size.

    :::{field} width
    :type: int
    :::
    """

def _Square_new(width):
    """Creates a Square.

    Args:
        width: {type}`int` width of square

    Returns:
        {type}`Square`
    """
    self = struct(
        area = lambda *a, **k: _Square_area(self, *a, **k),
        width = width
    )
    return self

def _Square_area(self, ):
   """Tells the area of the square."""
   return self.width * self.width

Square = struct(
  TYPEDEF = _Square_typedef,
  new = _Square_new,
  area = _Square_area,
)
```

This will then genereate markdown that looks like:

```
::::{bzl:typedef} Square
A square with fixed size

:::{bzl:field} width
:type: int
:::
:::{bzl:function} new()
...args etc from _Square_new...
:::
:::{bzl:function} area()
...args etc from _Square_area...
:::
::::
```

Which renders as:

:::{bzl:currentfile} //example:square.bzl
:::

::::{bzl:typedef} Square
A square with fixed size

:::{bzl:field} width
:type: int
:::
:::{bzl:function} new()
...
:::
:::{bzl:function} area()
...
:::
::::
