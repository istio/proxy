:::{default-domain} bzl
:::

:::{bzl:currentfile} //lang:function.bzl
:::


# Function

Module documentation

::::::{bzl:function} myfunc(foo, bar=False, baz=[]) -> FooObj

This is a bazel function.

:arg arg1:
  {default-value}`99`
  {type}`bool | int`
  arg1 doc

:arg arg2:
  {default-value}`True`
  {type}`dict[str, str]` my arg2 doc

  and a second paragraph of text here
:arg arg3:
  {default-value}`"arg3default"`
  {type}`list[int]`
  my arg3 doc
:arg arg4:
  my arg4 doc

:returns:
  {bzl:return-type}`list | int`
  description

:::{deprecated} unspecified

Some doc about the deprecation
:::

::::::

:::{bzl:function} mylongfunc(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)

:::
