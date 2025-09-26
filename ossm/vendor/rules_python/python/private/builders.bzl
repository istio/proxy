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

def _DepsetBuilder(order = None):
    """Create a builder for a depset.

    Args:
        order: {type}`str | None` The order to initialize the depset to, if any.

    Returns:
        {type}`DepsetBuilder`
    """

    # buildifier: disable=uninitialized
    self = struct(
        _order = [order],
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

# Skylib's types module doesn't have is_file, so roll our own
def _is_file(value):
    return type(value) == "File"

def _is_runfiles(value):
    return type(value) == "runfiles"

builders = struct(
    DepsetBuilder = _DepsetBuilder,
    RunfilesBuilder = _RunfilesBuilder,
)
