:::{default-domain} bzl
:::

:::{bzl:currentfile} //lang:rule.bzl
:::


# Rule

Here is some module documentation

Next, we're going to document some rules.

::::{bzl:rule} my_rule(ra1, ra2=3)

:attr ra1:
  {bzl:default-value}`//foo:bar`
  {type}`attr.label`
  Docs for attribute ra1.

  :::{bzl:attr-info} Info
  :executable: true
  :mandatory: true
  :::

  {required-providers}`"Display <//lang:provider.bzl%LangInfo>"`

:attr ra2:
  {type}`attr.label`
  Docs for attribute ra2

:provides: LangInfo

::::
