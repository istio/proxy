:::{default-domain} bzl
:::

:::{bzl:currentfile} //lang:provider.bzl
:::


# Provider

below is a provider

::::{bzl:provider} LangInfo

my provider doc

:::{bzl:function} LangInfo(mi1, mi2=None)

:arg ami1:
  {type}`depset[str]`
  mi1 doc
:arg ami2: ami2 doc
  {type}`None | depset[File]`
:::

:::{bzl:provider-field} mi1
:type: depset[str]

The doc for mi1
:::

:::{bzl:provider-field} mi2
:type: str
:::
::::
