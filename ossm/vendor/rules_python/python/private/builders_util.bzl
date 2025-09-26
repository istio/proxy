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

"""Utilities for builders."""

load("@bazel_skylib//lib:types.bzl", "types")

def to_label_maybe(value):
    """Converts `value` to a `Label`, maybe.

    The "maybe" qualification is because invalid values for `Label()`
    are returned as-is (e.g. None, or special values that might be
    used with e.g. the `default` attribute arg).

    Args:
        value: {type}`str | Label | None | object` the value to turn into a label,
            or return as-is.

    Returns:
        {type}`Label | input_value`
    """
    if value == None:
        return None
    if is_label(value):
        return value
    if types.is_string(value):
        return Label(value)
    return value

def is_label(obj):
    """Tell if an object is a `Label`."""
    return type(obj) == "Label"

def kwargs_set_default_ignore_none(kwargs, key, default):
    """Normalize None/missing to `default`."""
    existing = kwargs.get(key)
    if existing == None:
        kwargs[key] = default

def kwargs_set_default_list(kwargs, key):
    """Normalizes None/missing to list."""
    existing = kwargs.get(key)
    if existing == None:
        kwargs[key] = []

def kwargs_set_default_dict(kwargs, key):
    """Normalizes None/missing to list."""
    existing = kwargs.get(key)
    if existing == None:
        kwargs[key] = {}

def kwargs_set_default_doc(kwargs):
    """Sets the `doc` arg default."""
    existing = kwargs.get("doc")
    if existing == None:
        kwargs["doc"] = ""

def kwargs_set_default_mandatory(kwargs):
    """Sets `False` as the `mandatory` arg default."""
    existing = kwargs.get("mandatory")
    if existing == None:
        kwargs["mandatory"] = False

def kwargs_getter(kwargs, key):
    """Create a function to get `key` from `kwargs`."""
    return lambda: kwargs.get(key)

def kwargs_setter(kwargs, key):
    """Create a function to set `key` in `kwargs`."""

    def setter(v):
        kwargs[key] = v

    return setter

def kwargs_getter_doc(kwargs):
    """Creates a `kwargs_getter` for the `doc` key."""
    return kwargs_getter(kwargs, "doc")

def kwargs_setter_doc(kwargs):
    """Creates a `kwargs_setter` for the `doc` key."""
    return kwargs_setter(kwargs, "doc")

def kwargs_getter_mandatory(kwargs):
    """Creates a `kwargs_getter` for the `mandatory` key."""
    return kwargs_getter(kwargs, "mandatory")

def kwargs_setter_mandatory(kwargs):
    """Creates a `kwargs_setter` for the `mandatory` key."""
    return kwargs_setter(kwargs, "mandatory")

def list_add_unique(add_to, others):
    """Bulk add values to a list if not already present.

    Args:
        add_to: {type}`list[T]` the list to add values to. It is modified
            in-place.
        others: {type}`collection[collection[T]]` collection of collections of
            the values to add.
    """
    existing = {v: None for v in add_to}
    for values in others:
        for value in values:
            if value not in existing:
                add_to.append(value)
