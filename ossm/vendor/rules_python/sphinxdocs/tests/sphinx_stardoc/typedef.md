:::{default-domain} bzl
:::

:::{bzl:currentfile} //lang:typedef.bzl
:::


# Typedef

below is a provider

:::::::::{bzl:typedef} MyType

my type doc

:::{bzl:function} method(a, b)

:arg a:
  {type}`depset[str]`
  arg a doc
:arg b: ami2 doc
  {type}`None | depset[File]`
  arg b doc
:::

:::{bzl:field} field
:type: str

field doc
:::

:::::::::
