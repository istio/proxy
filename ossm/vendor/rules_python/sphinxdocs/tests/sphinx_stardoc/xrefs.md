:::{default-domain} bzl
:::

# Xrefs

Various tests of cross referencing support

## Short name

* function: {obj}`myfunc`
* function arg: {obj}`myfunc.arg1`
* rule: {obj}`my_rule`
* rule attr: {obj}`my_rule.ra1`
* provider: {obj}`LangInfo`
* tag class: {obj}`myext.mytag`

## Fully qualified label without repo

* function: {obj}`//lang:function.bzl%myfunc`
* function arg: {obj}`//lang:function.bzl%myfunc.arg1`
* rule: {obj}`//lang:rule.bzl%my_rule`
* rule attr: {obj}`//lang:rule.bzl%my_rule.ra1`
* provider: {obj}`//lang:provider.bzl%LangInfo`
* aspect: {obj}`//lang:aspect.bzl%myaspect`
* target: {obj}`//lang:relativetarget`

## Fully qualified label with repo

* function: {obj}`@testrepo//lang:function.bzl%myfunc`
* function arg: {obj}`@testrepo//lang:function.bzl%myfunc.arg1`
* rule: {obj}`@testrepo//lang:rule.bzl%my_rule`
* function: {obj}`@testrepo//lang:rule.bzl%my_rule.ra1`
* provider: {obj}`@testrepo//lang:provider.bzl%LangInfo`
* aspect: {obj}`@testrepo//lang:aspect.bzl%myaspect`
* target: {obj}`@testrepo//lang:relativetarget`

## Using origin keys

* provider using `{type}`: {type}`"@rules_python//sphinxdocs/tests/sphinx_stardoc:bzl_rule.bzl%GenericInfo"`

## Any xref

* {any}`LangInfo`
