# Copyright 2025 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Builders for creating rules, aspects et al.

When defining rules, Bazel only allows creating *immutable* objects that can't
be introspected. This makes it difficult to perform arbitrary customizations of
how a rule is defined, which makes extending a rule implementation prone to
copy/paste issues and version skew.

These builders are, essentially, mutable and inspectable wrappers for those
Bazel objects. This allows defining a rule where the values are mutable and
callers can customize them to derive their own variant of the rule while still
inheriting everything else about the rule.

To that end, the builders are not strict in how they handle values. They
generally assume that the values provided are valid and provide ways to
override their logic and force particular values to be used when they are
eventually converted to the args for calling e.g. `rule()`.

:::{important}
When using builders, most lists, dicts, et al passed into them **must** be
locally created values, otherwise they won't be mutable. This is due to Bazel's
implicit immutability rules: after evaluating a `.bzl` file, its global
variables are frozen.
:::

:::{tip}
To aid defining reusable pieces, many APIs accept no-arg callable functions
that create a builder. For example, common attributes can be stored
in a `dict[str, lambda]`, e.g. `ATTRS = {"srcs": lambda: LabelList(...)}`.
:::

Example usage:

```

load(":rule_builders.bzl", "ruleb")
load(":attr_builders.bzl", "attrb")

# File: foo_binary.bzl
_COMMON_ATTRS = {
    "srcs": lambda: attrb.LabelList(...),
}

def create_foo_binary_builder():
    foo = ruleb.Rule(
        executable = True,
    )
    foo.implementation.set(_foo_binary_impl)
    foo.attrs.update(COMMON_ATTRS)
    return foo

def create_foo_test_builder():
    foo = create_foo_binary_build()

    binary_impl = foo.implementation.get()
    def foo_test_impl(ctx):
      binary_impl(ctx)
      ...

    foo.implementation.set(foo_test_impl)
    foo.executable.set(False)
    foo.test.test(True)
    foo.attrs.update(
        _coverage = attrb.Label(default="//:coverage")
    )
    return foo

foo_binary = create_foo_binary_builder().build()
foo_test = create_foo_test_builder().build()

# File: custom_foo_binary.bzl
load(":foo_binary.bzl", "create_foo_binary_builder")

def create_custom_foo_binary():
    r = create_foo_binary_builder()
    r.attrs["srcs"].default.append("whatever.txt")
    return r.build()

custom_foo_binary = create_custom_foo_binary()
```

:::{versionadded} 1.3.0
:::
"""

load("@bazel_skylib//lib:types.bzl", "types")
load(
    ":builders_util.bzl",
    "kwargs_getter",
    "kwargs_getter_doc",
    "kwargs_set_default_dict",
    "kwargs_set_default_doc",
    "kwargs_set_default_ignore_none",
    "kwargs_set_default_list",
    "kwargs_setter",
    "kwargs_setter_doc",
    "list_add_unique",
    "normalize_transition_in_out_value",
    "normalize_transition_in_out_values",
)

# Various string constants for kwarg key names used across two or more
# functions, or in contexts with optional lookups (e.g. dict.dict, key in dict).
# Constants are used to reduce the chance of typos.
# NOTE: These keys are often part of function signature via `**kwargs`; they
# are not simply internal names.
_ATTRS = "attrs"
_CFG = "cfg"
_EXEC_COMPATIBLE_WITH = "exec_compatible_with"
_EXEC_GROUPS = "exec_groups"
_IMPLEMENTATION = "implementation"
_INPUTS = "inputs"
_OUTPUTS = "outputs"
_TOOLCHAINS = "toolchains"

def _is_builder(obj):
    return hasattr(obj, "build")

def _ExecGroup_typedef():
    """Builder for {external:bzl:obj}`exec_group`

    :::{function} toolchains() -> list[ToolchainType]
    :::

    :::{function} exec_compatible_with() -> list[str | Label]
    :::

    :::{include} /_includes/field_kwargs_doc.md
    :::
    """

def _ExecGroup_new(**kwargs):
    """Creates a builder for {external:bzl:obj}`exec_group`.

    Args:
        **kwargs: Same as {external:bzl:obj}`exec_group`

    Returns:
        {type}`ExecGroup`
    """
    kwargs_set_default_list(kwargs, _TOOLCHAINS)
    kwargs_set_default_list(kwargs, _EXEC_COMPATIBLE_WITH)

    for i, value in enumerate(kwargs[_TOOLCHAINS]):
        kwargs[_TOOLCHAINS][i] = _ToolchainType_maybe_from(value)

    # buildifier: disable=uninitialized
    self = struct(
        toolchains = kwargs_getter(kwargs, _TOOLCHAINS),
        exec_compatible_with = kwargs_getter(kwargs, _EXEC_COMPATIBLE_WITH),
        kwargs = kwargs,
        build = lambda: _ExecGroup_build(self),
    )
    return self

def _ExecGroup_maybe_from(obj):
    if types.is_function(obj):
        return obj()
    else:
        return obj

def _ExecGroup_build(self):
    kwargs = dict(self.kwargs)
    if kwargs.get(_TOOLCHAINS):
        kwargs[_TOOLCHAINS] = [
            v.build() if _is_builder(v) else v
            for v in kwargs[_TOOLCHAINS]
        ]
    if kwargs.get(_EXEC_COMPATIBLE_WITH):
        kwargs[_EXEC_COMPATIBLE_WITH] = [
            v.build() if _is_builder(v) else v
            for v in kwargs[_EXEC_COMPATIBLE_WITH]
        ]
    return exec_group(**kwargs)

# buildifier: disable=name-conventions
ExecGroup = struct(
    TYPEDEF = _ExecGroup_typedef,
    new = _ExecGroup_new,
    build = _ExecGroup_build,
)

def _ToolchainType_typedef():
    """Builder for {obj}`config_common.toolchain_type`

    :::{include} /_includes/field_kwargs_doc.md
    :::

    :::{function} mandatory() -> bool
    :::

    :::{function} name() -> str | Label | None
    :::

    :::{function} set_name(v: str)
    :::

    :::{function} set_mandatory(v: bool)
    :::
    """

def _ToolchainType_new(name = None, **kwargs):
    """Creates a builder for `config_common.toolchain_type`.

    Args:
        name: {type}`str | Label | None` the toolchain type target.
        **kwargs: Same as {obj}`config_common.toolchain_type`

    Returns:
        {type}`ToolchainType`
    """
    kwargs["name"] = name
    kwargs_set_default_ignore_none(kwargs, "mandatory", True)

    # buildifier: disable=uninitialized
    self = struct(
        # keep sorted
        build = lambda: _ToolchainType_build(self),
        kwargs = kwargs,
        mandatory = kwargs_getter(kwargs, "mandatory"),
        name = kwargs_getter(kwargs, "name"),
        set_mandatory = kwargs_setter(kwargs, "mandatory"),
        set_name = kwargs_setter(kwargs, "name"),
    )
    return self

def _ToolchainType_maybe_from(obj):
    if types.is_string(obj) or type(obj) == "Label":
        return ToolchainType.new(name = obj)
    elif types.is_function(obj):
        # A lambda to create a builder
        return obj()
    else:
        # For lack of another option, return it as-is.
        # Presumably it's already a builder or other valid object.
        return obj

def _ToolchainType_build(self):
    """Builds a `config_common.toolchain_type`

    Args:
        self: implicitly added

    Returns:
        {type}`toolchain_type`
    """
    kwargs = dict(self.kwargs)
    name = kwargs.pop("name")  # Name must be positional
    return config_common.toolchain_type(name, **kwargs)

# buildifier: disable=name-conventions
ToolchainType = struct(
    TYPEDEF = _ToolchainType_typedef,
    new = _ToolchainType_new,
    build = _ToolchainType_build,
)

def _RuleCfg_typedef():
    """Wrapper for `rule.cfg` arg.

    :::{function} implementation() -> str | callable | None | config.target | config.none
    :::

    ::::{function} inputs() -> list[Label]

    :::{seealso}
    The {obj}`add_inputs()` and {obj}`update_inputs` methods for adding unique
    values.
    :::
    ::::

    :::{function} outputs() -> list[Label]

    :::{seealso}
    The {obj}`add_outputs()` and {obj}`update_outputs` methods for adding unique
    values.
    :::
    :::

    :::{function} set_implementation(v: str | callable | None | config.target | config.none)

    The string values "target" and "none" are supported.
    :::
    """

def _RuleCfg_new(rule_cfg_arg):
    """Creates a builder for the `rule.cfg` arg.

    Args:
        rule_cfg_arg: {type}`str | dict | None` The `cfg` arg passed to Rule().

    Returns:
        {type}`RuleCfg`
    """
    state = {}
    if types.is_dict(rule_cfg_arg):
        state.update(rule_cfg_arg)
    else:
        # Assume its a string, config.target, config.none, or other
        # valid object.
        state[_IMPLEMENTATION] = rule_cfg_arg

    kwargs_set_default_list(state, _INPUTS)
    kwargs_set_default_list(state, _OUTPUTS)

    normalize_transition_in_out_values("input", state[_INPUTS])
    normalize_transition_in_out_values("output", state[_OUTPUTS])

    # buildifier: disable=uninitialized
    self = struct(
        add_inputs = lambda *a, **k: _RuleCfg_add_inputs(self, *a, **k),
        add_outputs = lambda *a, **k: _RuleCfg_add_outputs(self, *a, **k),
        _state = state,
        build = lambda: _RuleCfg_build(self),
        implementation = kwargs_getter(state, _IMPLEMENTATION),
        inputs = kwargs_getter(state, _INPUTS),
        outputs = kwargs_getter(state, _OUTPUTS),
        set_implementation = kwargs_setter(state, _IMPLEMENTATION),
        update_inputs = lambda *a, **k: _RuleCfg_update_inputs(self, *a, **k),
        update_outputs = lambda *a, **k: _RuleCfg_update_outputs(self, *a, **k),
    )
    return self

def _RuleCfg_add_inputs(self, *inputs):
    """Adds an input to the list of inputs, if not present already.

    :::{seealso}
    The {obj}`update_inputs()` method for adding a collection of
    values.
    :::

    Args:
        self: implicitly arg.
        *inputs: {type}`Label` the inputs to add. Note that a `Label`,
            not `str`, should be passed to ensure different apparent labels
            can be properly de-duplicated.
    """
    self.update_inputs(inputs)

def _RuleCfg_add_outputs(self, *outputs):
    """Adds an output to the list of outputs, if not present already.

    :::{seealso}
    The {obj}`update_outputs()` method for adding a collection of
    values.
    :::

    Args:
        self: implicitly arg.
        *outputs: {type}`Label` the outputs to add. Note that a `Label`,
            not `str`, should be passed to ensure different apparent labels
            can be properly de-duplicated.
    """
    self.update_outputs(outputs)

def _RuleCfg_build(self):
    """Builds the rule cfg into the value rule.cfg arg value.

    Returns:
        {type}`transition` the transition object to apply to the rule.
    """
    impl = self._state[_IMPLEMENTATION]
    if impl == "target" or impl == None:
        # config.target is Bazel 8+
        if hasattr(config, "target"):
            return config.target()
        else:
            return None
    elif impl == "none":
        return config.none()
    elif types.is_function(impl):
        return transition(
            implementation = impl,
            # Transitions only accept unique lists of strings.
            inputs = {str(v): None for v in self._state[_INPUTS]}.keys(),
            outputs = {str(v): None for v in self._state[_OUTPUTS]}.keys(),
        )
    else:
        # Assume its valid. Probably an `config.XXX` object or manually
        # set transition object.
        return impl

def _RuleCfg_update_inputs(self, *others):
    """Add a collection of values to inputs.

    Args:
        self: implicitly added
        *others: {type}`list[Label]` collection of labels to add to
            inputs. Only values not already present are added. Note that a
            `Label`, not `str`, should be passed to ensure different apparent
            labels can be properly de-duplicated.
    """
    list_add_unique(
        self._state[_INPUTS],
        others,
        convert = lambda v: normalize_transition_in_out_value("input", v),
    )

def _RuleCfg_update_outputs(self, *others):
    """Add a collection of values to outputs.

    Args:
        self: implicitly added
        *others: {type}`list[Label]` collection of labels to add to
            outputs. Only values not already present are added. Note that a
            `Label`, not `str`, should be passed to ensure different apparent
            labels can be properly de-duplicated.
    """
    list_add_unique(
        self._state[_OUTPUTS],
        others,
        convert = lambda v: normalize_transition_in_out_value("output", v),
    )

# buildifier: disable=name-conventions
RuleCfg = struct(
    TYPEDEF = _RuleCfg_typedef,
    new = _RuleCfg_new,
    # keep sorted
    add_inputs = _RuleCfg_add_inputs,
    add_outputs = _RuleCfg_add_outputs,
    build = _RuleCfg_build,
    update_inputs = _RuleCfg_update_inputs,
    update_outputs = _RuleCfg_update_outputs,
)

def _Rule_typedef():
    """A builder to accumulate state for constructing a `rule` object.

    :::{field} attrs
    :type: AttrsDict
    :::

    :::{field} cfg
    :type: RuleCfg
    :::

    :::{function} doc() -> str
    :::

    :::{function} exec_groups() -> dict[str, ExecGroup]
    :::

    :::{function} executable() -> bool
    :::

    :::{include} /_includes/field_kwargs_doc.md
    :::

    :::{function} fragments() -> list[str]
    :::

    :::{function} implementation() -> callable | None
    :::

    :::{function} provides() -> list[provider | list[provider]]
    :::

    :::{function} set_doc(v: str)
    :::

    :::{function} set_executable(v: bool)
    :::

    :::{function} set_implementation(v: callable)
    :::

    :::{function} set_test(v: bool)
    :::

    :::{function} test() -> bool
    :::

    :::{function} toolchains() -> list[ToolchainType]
    :::
    """

def _Rule_new(**kwargs):
    """Builder for creating rules.

    Args:
        **kwargs: The same as the `rule()` function, but using builders or
            dicts to specify sub-objects instead of the immutable Bazel
            objects.
    """
    kwargs.setdefault(_IMPLEMENTATION, None)
    kwargs_set_default_doc(kwargs)
    kwargs_set_default_dict(kwargs, _EXEC_GROUPS)
    kwargs_set_default_ignore_none(kwargs, "executable", False)
    kwargs_set_default_list(kwargs, "fragments")
    kwargs_set_default_list(kwargs, "provides")
    kwargs_set_default_ignore_none(kwargs, "test", False)
    kwargs_set_default_list(kwargs, _TOOLCHAINS)

    for name, value in kwargs[_EXEC_GROUPS].items():
        kwargs[_EXEC_GROUPS][name] = _ExecGroup_maybe_from(value)

    for i, value in enumerate(kwargs[_TOOLCHAINS]):
        kwargs[_TOOLCHAINS][i] = _ToolchainType_maybe_from(value)

    # buildifier: disable=uninitialized
    self = struct(
        attrs = _AttrsDict_new(kwargs.pop(_ATTRS, None)),
        build = lambda *a, **k: _Rule_build(self, *a, **k),
        cfg = _RuleCfg_new(kwargs.pop(_CFG, None)),
        doc = kwargs_getter_doc(kwargs),
        exec_groups = kwargs_getter(kwargs, _EXEC_GROUPS),
        executable = kwargs_getter(kwargs, "executable"),
        fragments = kwargs_getter(kwargs, "fragments"),
        implementation = kwargs_getter(kwargs, _IMPLEMENTATION),
        kwargs = kwargs,
        provides = kwargs_getter(kwargs, "provides"),
        set_doc = kwargs_setter_doc(kwargs),
        set_executable = kwargs_setter(kwargs, "executable"),
        set_implementation = kwargs_setter(kwargs, _IMPLEMENTATION),
        set_test = kwargs_setter(kwargs, "test"),
        test = kwargs_getter(kwargs, "test"),
        to_kwargs = lambda: _Rule_to_kwargs(self),
        toolchains = kwargs_getter(kwargs, _TOOLCHAINS),
    )
    return self

def _Rule_build(self, debug = ""):
    """Builds a `rule` object

    Args:
        self: implicitly added
        debug: {type}`str` If set, prints the args used to create the rule.

    Returns:
        {type}`rule`
    """
    kwargs = self.to_kwargs()
    if debug:
        lines = ["=" * 80, "rule kwargs: {}:".format(debug)]
        for k, v in sorted(kwargs.items()):
            if types.is_dict(v):
                lines.append("  %s={" % k)
                for k2, v2 in sorted(v.items()):
                    lines.append("    {}: {}".format(k2, v2))
                lines.append("  }")
            elif types.is_list(v):
                lines.append("  {}=[".format(k))
                for i, v2 in enumerate(v):
                    lines.append("    [{}] {}".format(i, v2))
                lines.append("  ]")
            else:
                lines.append("  {}={}".format(k, v))
        print("\n".join(lines))  # buildifier: disable=print
    return rule(**kwargs)

def _Rule_to_kwargs(self):
    """Builds the arguments for calling `rule()`.

    This is added as an escape hatch to construct the final values `rule()`
    kwarg values in case callers want to manually change them.

    Args:
        self: implicitly added.

    Returns:
        {type}`dict`
    """
    kwargs = dict(self.kwargs)
    if _EXEC_GROUPS in kwargs:
        kwargs[_EXEC_GROUPS] = {
            k: v.build() if _is_builder(v) else v
            for k, v in kwargs[_EXEC_GROUPS].items()
        }
    if _TOOLCHAINS in kwargs:
        kwargs[_TOOLCHAINS] = [
            v.build() if _is_builder(v) else v
            for v in kwargs[_TOOLCHAINS]
        ]
    if _ATTRS not in kwargs:
        kwargs[_ATTRS] = self.attrs.build()
    if _CFG not in kwargs:
        kwargs[_CFG] = self.cfg.build()
    return kwargs

# buildifier: disable=name-conventions
Rule = struct(
    TYPEDEF = _Rule_typedef,
    new = _Rule_new,
    build = _Rule_build,
    to_kwargs = _Rule_to_kwargs,
)

def _AttrsDict_typedef():
    """Builder for the dictionary of rule attributes.

    :::{field} map
    :type: dict[str, AttributeBuilder]

    The underlying dict of attributes. Directly accessible so that regular
    dict operations (e.g. `x in y`) can be performed, if necessary.
    :::

    :::{function} get(key, default=None)
    Get an entry from the dict. Convenience wrapper for `.map.get(...)`
    :::

    :::{function} items() -> list[tuple[str, object]]
    Returns a list of key-value tuples. Convenience wrapper for `.map.items()`
    :::

    :::{function} pop(key, default) -> object
    Removes a key from the attr dict
    :::
    """

def _AttrsDict_new(initial):
    """Creates a builder for the `rule.attrs` dict.

    Args:
        initial: {type}`dict[str, callable | AttributeBuilder] | None` dict of
            initial values to populate the attributes dict with.

    Returns:
        {type}`AttrsDict`
    """

    # buildifier: disable=uninitialized
    self = struct(
        # keep sorted
        build = lambda: _AttrsDict_build(self),
        get = lambda *a, **k: self.map.get(*a, **k),
        items = lambda: self.map.items(),
        map = {},
        put = lambda key, value: _AttrsDict_put(self, key, value),
        update = lambda *a, **k: _AttrsDict_update(self, *a, **k),
        pop = lambda *a, **k: self.map.pop(*a, **k),
    )
    if initial:
        _AttrsDict_update(self, initial)
    return self

def _AttrsDict_put(self, name, value):
    """Sets a value in the attrs dict.

    Args:
        self: implicitly added
        name: {type}`str` the attribute name to set in the dict
        value: {type}`AttributeBuilder | callable` the value for the
            attribute. If a callable, then it is treated as an
            attribute builder factory (no-arg callable that returns an
            attribute builder) and is called immediately.
    """
    if types.is_function(value):
        # Convert factory function to builder
        value = value()
    self.map[name] = value

def _AttrsDict_update(self, other):
    """Merge `other` into this object.

    Args:
        self: implicitly added
        other: {type}`dict[str, callable | AttributeBuilder]` the values to
            merge into this object. If the value a function, it is called
            with no args and expected to return an attribute builder. This
            allows defining dicts of common attributes (where the values are
            functions that create a builder) and merge them into the rule.
    """
    for k, v in other.items():
        # Handle factory functions that create builders
        if types.is_function(v):
            self.map[k] = v()
        else:
            self.map[k] = v

def _AttrsDict_build(self):
    """Build an attribute dict for passing to `rule()`.

    Returns:
        {type}`dict[str, Attribute]` where the values are `attr.XXX` objects
    """
    attrs = {}
    for k, v in self.map.items():
        attrs[k] = v.build() if _is_builder(v) else v
    return attrs

def _AttributeBuilder_typedef():
    """An abstract base typedef for builder for a Bazel {obj}`Attribute`

    Instances of this are a builder for a particular `Attribute` type,
    e.g. `attr.label`, `attr.string`, etc.
    """

# buildifier: disable=name-conventions
AttributeBuilder = struct(
    TYPEDEF = _AttributeBuilder_typedef,
)

# buildifier: disable=name-conventions
AttrsDict = struct(
    TYPEDEF = _AttrsDict_typedef,
    new = _AttrsDict_new,
    update = _AttrsDict_update,
    build = _AttrsDict_build,
)

ruleb = struct(
    Rule = _Rule_new,
    ToolchainType = _ToolchainType_new,
    ExecGroup = _ExecGroup_new,
)
