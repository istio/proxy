# Bazel plugin for Sphinx

The `sphinx_bzl` Python package is a Sphinx plugin that defines a custom domain
("bzl") in the Sphinx system. This provides first-class integration with Sphinx
and allows code comments to provide rich information and allows manually writing
docs for objects that aren't directly representable in bzl source code. For
example, the fields of a provider can use `:type:` to indicate the type of a
field, or manually written docs can use the `{bzl:target}` directive to document
a well known target.

## Configuring Sphinx

To enable the plugin in Sphinx, depend on
`@rules_python//sphinxdocs/src/sphinx_bzl` and enable it in `conf.py`:

```
extensions = [
    "sphinx_bzl.bzl",
]
```

## Brief introduction to Sphinx terminology

To aid understanding how to write docs, lets define a few common terms:

* **Role**: A role is the "bzl:obj" part when writing ``{bzl:obj}`ref` ``.
  Roles mark inline text as needing special processing. There's generally
  two types of processing: creating cross references, or role-specific custom
  rendering. For example `{bzl:obj}` will create a cross references, while
  `{bzl:default-value}` indicates the default value of an argument.
* **Directive**: A directive is indicated with `:::` and allows defining an
  entire object and its parts. For example, to describe a function and its
  arguments, the `:::{bzl:function}` directive is used.
* **Directive Option**: A directive option is the "type" part when writing
  `:type:` within a directive. Directive options are how directives are told
  the meaning of certain values, such as the type of a provider field. Depending
  on the object being documented, a directive option may be used instead of
  special role to indicate semantic values.

Most often, you'll be using roles to refer other objects or indicate special
values in doc strings. For directives, you're likely to only use them when
manually writing docs to document flags, targets, or other objects that
`sphinx_stardoc` generates for you.

## MyST vs RST

By default, Sphinx uses ReStructured Text (RST) syntax for its documents.
Unfortunately, RST syntax is very different than the popular Markdown syntax. To
bridge the gap, MyST translates Markdown-style syntax into the RST equivalents.
This allows easily using Markdown in bzl files.

While MyST isn't required for the core `sphinx_bzl` plugin to work, this
document uses MyST syntax because `sphinx_stardoc` bzl doc gen rule requires
MyST.

The main difference in syntax is:
* MyST directives use `:::{name}` with closing `:::` instead of `.. name::` with
  indented content.
* MyST roles use `{role:name}` instead of `:role:name:`

## Type expressions

Several roles or fields accept type expressions. Type expressions use
Python-style annotation syntax to describe data types. For example `None | list[str]`
describes a type of "None or a list of strings". Each component of the
expression is parsed and cross reference to its associated type definition.

## Cross references

In brief, to reference bzl objects, use the `bzl:obj` role and use the
Bazel label string you would use to refer to the object in Bazel (using `%` to
denote names within a file). For example, to unambiguously refer to `py_binary`:

```
{bzl:obj}`@rules_python//python:py_binary.bzl%py_binary`
```

The above is pretty long, so shorter names are also supported, and `sphinx_bzl`
will try to find something that matches. Additionally, in `.bzl` code, the
`bzl:` prefix is set as the default. The above can then be shortened to:

```
{obj}`py_binary`
```

The text that is displayed can be customized by putting the reference string in
chevrons (`<>`):

```
{obj}`the binary rule <py_binary>`
```

Specific types of objects (rules, functions, providers, etc) can be
specified to help disambiguate short names:

```
{function}`py_binary`  # Refers to the wrapping macro
{rule}`py_binary`  # Refers to the underlying rule
```

Finally, objects built into Bazel can be explicitly referenced by forcing
a lookup outside the local project using `{external}`. For example, the symbol
`toolchain` is a builtin Bazel function, but it could also be the name of a tag
class in the local project. To force looking up the builtin Bazel `toolchain` rule,
`{external:bzl:rule}` can be used, e.g.:

```
{external:bzl:obj}`toolchain`
```

Those are the basics of cross referencing. Sphinx has several additional
syntaxes for finding and referencing objects; see
[the MyST docs for supported
syntaxes](https://myst-parser.readthedocs.io/en/latest/syntax/cross-referencing.html#reference-roles)

### Cross reference roles

A cross reference role is the `obj` portion of `{bzl:obj}`. It affects what is
searched and matched.

:::{note}
The documentation renders using RST notation (`:foo:role:`), not
MyST notation (`{foo:role}`.
:::

:::{rst:role} bzl:arg
Refer to a function argument.
:::

:::{rst:role} bzl:attr
Refer to a rule attribute.
:::

:::{rst:role} bzl:flag
Refer to a flag.
:::

:::{rst:role} bzl:obj
Refer to any type of Bazel object
:::

:::{rst:role} bzl:rule
Refer to a rule.
:::

:::{rst:role} bzl:target
Refer to a target.
:::

:::{rst:role} bzl:type
Refer to a type or type expression; can also be used in argument documentation.

```
def func(arg):
    """Do stuff

    Args:
      arg: {type}`int | str` the arg
    """
    print(arg + 1)
```
:::

## Special roles

There are several special roles that can be used to annotate parts of objects,
such as the type of arguments or their default values.

:::{note}
The documentation renders using RST notation (`:foo:role:`), not
MyST notation (`{foo:role}`.
:::

:::{rst:role} bzl:default-value

Indicate the default value for a function argument or rule attribute. Use it in
the Args doc of a function or the doc text of an attribute.

```
def func(arg=1):
   """Do stuff

   Args:
     foo: {default-value}`1` the arg

my_rule = rule(attrs = {
    "foo": attr.string(doc="{default-value}`bar`)
})

```
:::

:::{rst:role} bzl:return-type

Indicates the return type for a function. Use it in the Returns doc of a
function.

```
def func():
    """Do stuff

    Returns:
      {return-type}`int`
    """
    return 1
```
:::

## Directives

Most directives are automatically generated by `sphinx_stardoc`. Here, we only
document ones that must be manually written.

To write a directive, a line starts with 3 to 6 colons (`:`), followed by the
directive name in braces (`{}`), and eventually ended by the same number of
colons on their own line. For example:

```
:::{bzl:target} //my:target

Doc about target
:::
```

:::{note}
The documentation renders using RST notation (`.. directive::`), not
MyST notation.
:::

Directives can be nested, but [the inner directives must have **fewer** colons
than outer
directives](https://myst-parser.readthedocs.io/en/latest/syntax/roles-and-directives.html#nesting-directives).


:::{rst:directive} .. bzl:currentfile:: file

This directive indicates the Bazel file that objects defined in the current
documentation file are in. This is required for any page that defines Bazel
objects. The format of `file` is Bazel label syntax, e.g. `//foo:bar.bzl` for bzl
files, and `//foo:BUILD.bazel` for things in BUILD files.

:::


:::::{rst:directive} .. bzl:target:: target

Documents a target. It takes no directive options. The format of `target`
can either be a fully qualified label (`//foo:bar`), or the base target name
relative to `{bzl:currentfile}`.

````
:::{bzl:target} //foo:target

My docs
:::
````

:::::

:::{rst:directive} .. bzl:flag:: target

Documents a flag. It has the same format as `{bzl:target}`
:::

::::::{rst:directive} .. bzl:typedef:: typename

Documents a user-defined structural "type".  These are typically generated by
the {obj}`sphinx_stardoc` rule after following [User-defined types] to create a
struct with a `TYPEDEF` field, but can also be manually defined if there's
no natural place for it in code, e.g. some ad-hoc structural type.

`````
::::{bzl:typedef} Square
Doc about Square

:::{bzl:field} width
:type: int
:::

:::{bzl:function} new(size)
  ...
:::

:::{bzl:function} area()
  ...
:::
::::
`````

Note that MyST requires the number of colons for the outer typedef directive
to be greater than the inner directives. Otherwise, only the first nested
directive is parsed as part of the typedef, but subsequent ones are not.
::::::

:::::{rst:directive} .. bzl:field:: fieldname

Documents a field of an object. These are nested within some other directive,
typically `{bzl:typedef}`

Directive options:
* `:type:` specifies the type of the field

````
:::{bzl:field} fieldname
:type: int | None | str

Doc about field
:::
````
:::::

:::::{rst:directive} .. bzl:provider-field:: fieldname

Documents a field of a provider. The directive itself is autogenerated by
`sphinx_stardoc`, but the content is simply the documentation string specified
in the provider's field.

Directive options:
* `:type:` specifies the type of the field

````
:::{bzl:provider-field} fieldname
:type: depset[File] | None

Doc about the provider field
:::
````
:::::
