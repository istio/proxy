# Copyright 2024 The Bazel Authors. All rights reserved.
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
"""Builders to make building complex objects easier."""

load("@bazel_skylib//lib:types.bzl", "types")

def _DepsetBuilder():
    """Create a builder for a depset."""

    # buildifier: disable=uninitialized
    self = struct(
        _order = [None],
        add = lambda *a, **k: _DepsetBuilder_add(self, *a, **k),
        build = lambda *a, **k: _DepsetBuilder_build(self, *a, **k),
        direct = [],
        get_order = lambda *a, **k: _DepsetBuilder_get_order(self, *a, **k),
        set_order = lambda *a, **k: _DepsetBuilder_set_order(self, *a, **k),
        transitive = [],
    )
    return self

def _DepsetBuilder_add(self, *values):
    """Add value to the depset.

    Args:
        self: {type}`DepsetBuilder` implicitly added.
        *values: {type}`depset | list | object` Values to add to the depset.
            The values can be a depset, the non-depset value to add, or
            a list of such values to add.

    Returns:
        {type}`DepsetBuilder`
    """
    for value in values:
        if types.is_list(value):
            for sub_value in value:
                if types.is_depset(sub_value):
                    self.transitive.append(sub_value)
                else:
                    self.direct.append(sub_value)
        elif types.is_depset(value):
            self.transitive.append(value)
        else:
            self.direct.append(value)
    return self

def _DepsetBuilder_set_order(self, order):
    """Sets the order to use.

    Args:
        self: {type}`DepsetBuilder` implicitly added.
        order: {type}`str` One of the {obj}`depset` `order` values.

    Returns:
        {type}`DepsetBuilder`
    """
    self._order[0] = order
    return self

def _DepsetBuilder_get_order(self):
    """Gets the depset order that will be used.

    Args:
        self: {type}`DepsetBuilder` implicitly added.

    Returns:
        {type}`str | None` If not previously set, `None` is returned.
    """
    return self._order[0]

def _DepsetBuilder_build(self):
    """Creates a {obj}`depset` from the accumulated values.

    Args:
        self: {type}`DepsetBuilder` implicitly added.

    Returns:
        {type}`depset`
    """
    if not self.direct and len(self.transitive) == 1 and self._order[0] == None:
        return self.transitive[0]
    else:
        kwargs = {}
        if self._order[0] != None:
            kwargs["order"] = self._order[0]
        return depset(direct = self.direct, transitive = self.transitive, **kwargs)

def _Optional(*initial):
    """A wrapper for a re-assignable value that may or may not be set.

    This allows structs to have attributes that aren't inherently mutable
    and must be re-assigned to have their value updated.

    Args:
        *initial: A single vararg to be the initial value, or no args
            to leave it unset.

    Returns:
        {type}`Optional`
    """
    if len(initial) > 1:
        fail("Only zero or one positional arg allowed")

    # buildifier: disable=uninitialized
    self = struct(
        _value = list(initial),
        present = lambda *a, **k: _Optional_present(self, *a, **k),
        set = lambda *a, **k: _Optional_set(self, *a, **k),
        get = lambda *a, **k: _Optional_get(self, *a, **k),
    )
    return self

def _Optional_set(self, value):
    """Sets the value of the optional.

    Args:
        self: implicitly added
        value: the value to set.
    """
    if len(self._value) == 0:
        self._value.append(value)
    else:
        self._value[0] = value

def _Optional_get(self):
    """Gets the value of the optional, or error.

    Args:
        self: implicitly added

    Returns:
        The stored value, or error if not set.
    """
    if not len(self._value):
        fail("Value not present")
    return self._value[0]

def _Optional_present(self):
    """Tells if a value is present.

    Args:
        self: implicitly added

    Returns:
        {type}`bool` True if the value is set, False if not.
    """
    return len(self._value) > 0

def _RuleBuilder(implementation = None, **kwargs):
    """Builder for creating rules.

    Args:
        implementation: {type}`callable` The rule implementation function.
        **kwargs: The same as the `rule()` function, but using builders
            for the non-mutable Bazel objects.
    """

    # buildifier: disable=uninitialized
    self = struct(
        attrs = dict(kwargs.pop("attrs", None) or {}),
        cfg = kwargs.pop("cfg", None) or _TransitionBuilder(),
        exec_groups = dict(kwargs.pop("exec_groups", None) or {}),
        executable = _Optional(),
        fragments = list(kwargs.pop("fragments", None) or []),
        implementation = _Optional(implementation),
        extra_kwargs = kwargs,
        provides = list(kwargs.pop("provides", None) or []),
        test = _Optional(),
        toolchains = list(kwargs.pop("toolchains", None) or []),
        build = lambda *a, **k: _RuleBuilder_build(self, *a, **k),
        to_kwargs = lambda *a, **k: _RuleBuilder_to_kwargs(self, *a, **k),
    )
    if "test" in kwargs:
        self.test.set(kwargs.pop("test"))
    if "executable" in kwargs:
        self.executable.set(kwargs.pop("executable"))
    return self

def _RuleBuilder_build(self, debug = ""):
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
            lines.append("  {}={}".format(k, v))
        print("\n".join(lines))  # buildifier: disable=print
    return rule(**kwargs)

def _RuleBuilder_to_kwargs(self):
    """Builds the arguments for calling `rule()`.

    Args:
        self: implicitly added

    Returns:
        {type}`dict`
    """
    kwargs = {}
    if self.executable.present():
        kwargs["executable"] = self.executable.get()
    if self.test.present():
        kwargs["test"] = self.test.get()

    kwargs.update(
        implementation = self.implementation.get(),
        cfg = self.cfg.build() if self.cfg.implementation.present() else None,
        attrs = {
            k: (v.build() if hasattr(v, "build") else v)
            for k, v in self.attrs.items()
        },
        exec_groups = self.exec_groups,
        fragments = self.fragments,
        provides = self.provides,
        toolchains = self.toolchains,
    )
    kwargs.update(self.extra_kwargs)
    return kwargs

def _RunfilesBuilder():
    """Creates a `RunfilesBuilder`.

    Returns:
        {type}`RunfilesBuilder`
    """

    # buildifier: disable=uninitialized
    self = struct(
        add = lambda *a, **k: _RunfilesBuilder_add(self, *a, **k),
        add_targets = lambda *a, **k: _RunfilesBuilder_add_targets(self, *a, **k),
        build = lambda *a, **k: _RunfilesBuilder_build(self, *a, **k),
        files = _DepsetBuilder(),
        root_symlinks = {},
        runfiles = [],
        symlinks = {},
    )
    return self

def _RunfilesBuilder_add(self, *values):
    """Adds a value to the runfiles.

    Args:
        self: {type}`RunfilesBuilder` implicitly added.
        *values: {type}`File | runfiles | list[File] | depset[File] | list[runfiles]`
            The values to add.

    Returns:
        {type}`RunfilesBuilder`
    """
    for value in values:
        if types.is_list(value):
            for sub_value in value:
                _RunfilesBuilder_add_internal(self, sub_value)
        else:
            _RunfilesBuilder_add_internal(self, value)
    return self

def _RunfilesBuilder_add_targets(self, targets):
    """Adds runfiles from targets

    Args:
        self: {type}`RunfilesBuilder` implicitly added.
        targets: {type}`list[Target]` targets whose default runfiles
            to add.

    Returns:
        {type}`RunfilesBuilder`
    """
    for t in targets:
        self.runfiles.append(t[DefaultInfo].default_runfiles)
    return self

def _RunfilesBuilder_add_internal(self, value):
    if _is_file(value):
        self.files.add(value)
    elif types.is_depset(value):
        self.files.add(value)
    elif _is_runfiles(value):
        self.runfiles.append(value)
    else:
        fail("Unhandled value: type {}: {}".format(type(value), value))

def _RunfilesBuilder_build(self, ctx, **kwargs):
    """Creates a {obj}`runfiles` from the accumulated values.

    Args:
        self: {type}`RunfilesBuilder` implicitly added.
        ctx: {type}`ctx` The rule context to use to create the runfiles object.
        **kwargs: additional args to pass along to {obj}`ctx.runfiles`.

    Returns:
        {type}`runfiles`
    """
    return ctx.runfiles(
        transitive_files = self.files.build(),
        symlinks = self.symlinks,
        root_symlinks = self.root_symlinks,
        **kwargs
    ).merge_all(self.runfiles)

def _SetBuilder(initial = None):
    """Builder for list of unique values.

    Args:
        initial: {type}`list | None` The initial values.

    Returns:
        {type}`SetBuilder`
    """
    initial = {} if not initial else {v: None for v in initial}

    # buildifier: disable=uninitialized
    self = struct(
        # TODO - Switch this to use set() builtin when available
        # https://bazel.build/rules/lib/core/set
        _values = initial,
        update = lambda *a, **k: _SetBuilder_update(self, *a, **k),
        build = lambda *a, **k: _SetBuilder_build(self, *a, **k),
    )
    return self

def _SetBuilder_build(self):
    """Builds the values into a list

    Returns:
        {type}`list`
    """
    return self._values.keys()

def _SetBuilder_update(self, *others):
    """Adds values to the builder.

    Args:
        self: implicitly added
        *others: {type}`list` values to add to the set.
    """
    for other in others:
        for value in other:
            if value not in self._values:
                self._values[value] = None

def _TransitionBuilder(implementation = None, inputs = None, outputs = None, **kwargs):
    """Builder for transition objects.

    Args:
        implementation: {type}`callable` the transition implementation function.
        inputs: {type}`list[str]` the inputs for the transition.
        outputs: {type}`list[str]` the outputs of the transition.
        **kwargs: Extra keyword args to use when building.

    Returns:
        {type}`TransitionBuilder`
    """

    # buildifier: disable=uninitialized
    self = struct(
        implementation = _Optional(implementation),
        # Bazel requires transition.inputs to have unique values, so use set
        # semantics so extenders of a transition can easily add/remove values.
        # TODO - Use set builtin instead of custom builder, when available.
        # https://bazel.build/rules/lib/core/set
        inputs = _SetBuilder(inputs),
        # Bazel requires transition.inputs to have unique values, so use set
        # semantics so extenders of a transition can easily add/remove values.
        # TODO - Use set builtin instead of custom builder, when available.
        # https://bazel.build/rules/lib/core/set
        outputs = _SetBuilder(outputs),
        extra_kwargs = kwargs,
        build = lambda *a, **k: _TransitionBuilder_build(self, *a, **k),
    )
    return self

def _TransitionBuilder_build(self):
    """Creates a transition from the builder.

    Returns:
        {type}`transition`
    """
    return transition(
        implementation = self.implementation.get(),
        inputs = self.inputs.build(),
        outputs = self.outputs.build(),
        **self.extra_kwargs
    )

# Skylib's types module doesn't have is_file, so roll our own
def _is_file(value):
    return type(value) == "File"

def _is_runfiles(value):
    return type(value) == "runfiles"

builders = struct(
    DepsetBuilder = _DepsetBuilder,
    RunfilesBuilder = _RunfilesBuilder,
    RuleBuilder = _RuleBuilder,
    TransitionBuilder = _TransitionBuilder,
    SetBuilder = _SetBuilder,
    Optional = _Optional,
)
