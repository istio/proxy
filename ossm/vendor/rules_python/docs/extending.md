# Extending the rules

:::{important}
**This is public, but volatile, functionality.**

Extending and customizing the rules is supported functionality, but with weaker
backwards compatibility guarantees, and is not fully subject to the normal
backwards compatibility procedures and policies. It's simply not feasible to
support every possible customization with strong backwards compatibility
guarantees.
:::

Because of the rich ecosystem of tools and variety of use cases, APIs are
provided to make it easy to create custom rules using the existing rules as a
basis. This allows implementing behaviors that aren't possible using
wrapper macros around the core rules, and can make certain types of changes
much easier and transparent to implement.

:::{note}
It is not required to extend a core rule. The minimum requirement for a custom
rule is to return the appropriate provider (e.g. {bzl:obj}`PyInfo` etc).
Extending the core rules is most useful when you want all or most of the
behavior of a core rule.
:::

Follow or comment on https://github.com/bazel-contrib/rules_python/issues/1647
for the development of APIs to support custom derived rules.

## Creating custom rules

Custom rules can be created using the core rules as a basis by using their rule
builder APIs.

* [`//python/apis:executables.bzl`](#python-apis-executables-bzl): builders for
  executables.
* [`//python/apis:libraries.bzl`](#python-apis-libraries-bzl): builders for
  libraries.

These builders create {bzl:obj}`ruleb.Rule` objects, which are thin
wrappers around the keyword arguments eventually passed to the `rule()`
function. These builder APIs give access to the _entire_ rule definition and
allow arbitrary modifications.

This level of control is powerful but also volatile. A rule definition
contains many details that _must_ change as the implementation changes. What
is more or less likely to change isn't known in advance, but some general
rules of thumb are:

* Additive behavior to public attributes will be less prone to breaking.
* Internal attributes that directly support a public attribute are likely
  reliable.
* Internal attributes that support an action are more likely to change.
* Rule toolchains are moderately stable (toolchains are mostly internal to
  how a rule works, but custom toolchains are supported).

## Example: validating a source file

In this example, we derive a custom rule from `py_library` that verifies source
code contains the word "snakes". It does this by:

* Adding an implicit dependency on a checker program
* Calling the base implementation function
* Running the checker on the srcs files
* Adding the result to the `_validation` output group (a special output
  group for validation behaviors).

To users, they can use `has_snakes_library` the same as `py_library`. The same
is true for other targets that might consume the rule.

```
load("@rules_python//python/api:libraries.bzl", "libraries")
load("@rules_python//python/api:attr_builders.bzl", "attrb")

def _has_snakes_impl(ctx, base):
    providers = base(ctx)

    out = ctx.actions.declare_file(ctx.label.name + "_snakes.check")
    ctx.actions.run(
        inputs = ctx.files.srcs,
        outputs = [out],
        executable = ctx.attr._checker[DefaultInfo].files_to_run,
        args = [out.path] + [f.path for f in ctx.files.srcs],
    )
    prior_ogi = None
    for i, p in enumerate(providers):
        if type(p) == "OutputGroupInfo":
            prior_ogi = (i, p)
            break
    if prior_ogi:
        groups = {k: getattr(prior_ogi[1], k) for k in dir(prior_ogi)}
        if "_validation" in groups:
            groups["_validation"] = depset([out], transitive=groups["_validation"])
        else:
            groups["_validation"] = depset([out])
        providers[prior_ogi[0]] = OutputGroupInfo(**groups)
    else:
        providers.append(OutputGroupInfo(_validation=depset([out])))
    return providers

def create_has_snakes_rule():
    r = libraries.py_library_builder()
    base_impl = r.implementation()
    r.set_implementation(lambda ctx: _has_snakes_impl(ctx, base_impl))
    r.attrs["_checker"] = attrb.Label(
        default="//:checker",
        executable = True,
    )
    return r.build()
has_snakes_library = create_has_snakes_rule()
```

## Example: adding transitions

In this example, we derive a custom rule from `py_binary` to force building for a particular
platform. We do this by:

* Adding an additional output to the rule's cfg
* Calling the base transition function
* Returning the new transition outputs

```starlark

load("@rules_python//python/api:executables.bzl", "executables")

def _force_linux_impl(settings, attr, base_impl):
    settings = base_impl(settings, attr)
    settings["//command_line_option:platforms"] = ["//my/platforms:linux"]
    return settings

def create_rule():
    r = executables.py_binary_rule_builder()
    base_impl = r.cfg.implementation()
    r.cfg.set_implementation(
        lambda settings, attr: _force_linux_impl(settings, attr, base_impl)
    )
    r.cfg.add_output("//command_line_option:platforms")
    return r.build()

py_linux_binary = create_rule()
```

Users can then use `py_linux_binary` the same as a regular `py_binary`. It will
act as if `--platforms=//my/platforms:linux` was specified when building it.
