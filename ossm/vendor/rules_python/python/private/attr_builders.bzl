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

"""Builders for creating attributes et al.

:::{versionadded} 1.3.0
:::
"""

load("@bazel_skylib//lib:types.bzl", "types")
load(
    ":builders_util.bzl",
    "kwargs_getter",
    "kwargs_getter_doc",
    "kwargs_getter_mandatory",
    "kwargs_set_default_doc",
    "kwargs_set_default_ignore_none",
    "kwargs_set_default_list",
    "kwargs_set_default_mandatory",
    "kwargs_setter",
    "kwargs_setter_doc",
    "kwargs_setter_mandatory",
    "to_label_maybe",
)

# Various string constants for kwarg key names used across two or more
# functions, or in contexts with optional lookups (e.g. dict.dict, key in dict).
# Constants are used to reduce the chance of typos.
# NOTE: These keys are often part of function signature via `**kwargs`; they
# are not simply internal names.
_ALLOW_FILES = "allow_files"
_ALLOW_EMPTY = "allow_empty"
_ALLOW_SINGLE_FILE = "allow_single_file"
_DEFAULT = "default"
_INPUTS = "inputs"
_OUTPUTS = "outputs"
_CFG = "cfg"
_VALUES = "values"

def _kwargs_set_default_allow_empty(kwargs):
    existing = kwargs.get(_ALLOW_EMPTY)
    if existing == None:
        kwargs[_ALLOW_EMPTY] = True

def _kwargs_getter_allow_empty(kwargs):
    return kwargs_getter(kwargs, _ALLOW_EMPTY)

def _kwargs_setter_allow_empty(kwargs):
    return kwargs_setter(kwargs, _ALLOW_EMPTY)

def _kwargs_set_default_allow_files(kwargs):
    existing = kwargs.get(_ALLOW_FILES)
    if existing == None:
        kwargs[_ALLOW_FILES] = False

def _kwargs_getter_allow_files(kwargs):
    return kwargs_getter(kwargs, _ALLOW_FILES)

def _kwargs_setter_allow_files(kwargs):
    return kwargs_setter(kwargs, _ALLOW_FILES)

def _kwargs_set_default_aspects(kwargs):
    kwargs_set_default_list(kwargs, "aspects")

def _kwargs_getter_aspects(kwargs):
    return kwargs_getter(kwargs, "aspects")

def _kwargs_getter_providers(kwargs):
    return kwargs_getter(kwargs, "providers")

def _kwargs_set_default_providers(kwargs):
    kwargs_set_default_list(kwargs, "providers")

def _common_label_build(self, attr_factory):
    kwargs = dict(self.kwargs)
    kwargs[_CFG] = self.cfg.build()
    return attr_factory(**kwargs)

def _WhichCfg_typedef():
    """Values returned by `AttrCfg.which_cfg`

    :::{field} TARGET

    Indicates the target config is set.
    :::

    :::{field} EXEC

    Indicates the exec config is set.
    :::
    :::{field} NONE

    Indicates the "none" config is set (see {obj}`config.none`).
    :::
    :::{field} IMPL

    Indicates a custom transition is set.
    :::
    """

# buildifier: disable=name-conventions
_WhichCfg = struct(
    TYPEDEF = _WhichCfg_typedef,
    TARGET = "target",
    EXEC = "exec",
    NONE = "none",
    IMPL = "impl",
)

def _AttrCfg_typedef():
    """Builder for `cfg` arg of label attributes.

    :::{function} inputs() -> list[Label]
    :::

    :::{function} outputs() -> list[Label]
    :::

    :::{function} which_cfg() -> attrb.WhichCfg

    Tells which of the cfg modes is set. Will be one of: target, exec, none,
    or implementation
    :::
    """

_ATTR_CFG_WHICH = "which"
_ATTR_CFG_VALUE = "value"

def _AttrCfg_new(
        inputs = None,
        outputs = None,
        **kwargs):
    """Creates a builder for the `attr.cfg` attribute.

    Args:
        inputs: {type}`list[Label] | None` inputs to use for a transition
        outputs: {type}`list[Label] | None` outputs to use for a transition
        **kwargs: {type}`dict` Three different keyword args are supported.
            The presence of a keyword arg will mark the respective mode
            returned by `which_cfg`.
            - `cfg`: string of either "target" or "exec"
            - `exec_group`: string of an exec group name to use. None means
              to use regular exec config (i.e. `config.exec()`)
            - `implementation`: callable for a custom transition function.

    Returns:
        {type}`AttrCfg`
    """
    state = {
        _INPUTS: inputs,
        _OUTPUTS: outputs,
        # Value depends on _ATTR_CFG_WHICH key. See associated setters.
        _ATTR_CFG_VALUE: True,
        # str: one of the _WhichCfg values
        _ATTR_CFG_WHICH: _WhichCfg.TARGET,
    }
    kwargs_set_default_list(state, _INPUTS)
    kwargs_set_default_list(state, _OUTPUTS)

    # buildifier: disable=uninitialized
    self = struct(
        # keep sorted
        _state = state,
        build = lambda: _AttrCfg_build(self),
        exec_group = lambda: _AttrCfg_exec_group(self),
        implementation = lambda: _AttrCfg_implementation(self),
        inputs = kwargs_getter(state, _INPUTS),
        none = lambda: _AttrCfg_none(self),
        outputs = kwargs_getter(state, _OUTPUTS),
        set_exec = lambda *a, **k: _AttrCfg_set_exec(self, *a, **k),
        set_implementation = lambda *a, **k: _AttrCfg_set_implementation(self, *a, **k),
        set_none = lambda: _AttrCfg_set_none(self),
        set_target = lambda: _AttrCfg_set_target(self),
        target = lambda: _AttrCfg_target(self),
        which_cfg = kwargs_getter(state, _ATTR_CFG_WHICH),
    )

    # Only one of the three kwargs should be present. We just process anything
    # we see because it's simpler.
    if _CFG in kwargs:
        cfg = kwargs.pop(_CFG)
        if cfg == "target" or cfg == None:
            self.set_target()
        elif cfg == "exec":
            self.set_exec()
        elif cfg == "none":
            self.set_none()
        else:
            self.set_implementation(cfg)
    if "exec_group" in kwargs:
        self.set_exec(kwargs.pop("exec_group"))

    if "implementation" in kwargs:
        self.set_implementation(kwargs.pop("implementation"))

    return self

def _AttrCfg_from_attr_kwargs_pop(attr_kwargs):
    """Creates a `AttrCfg` from the cfg arg passed to an attribute bulider.

    Args:
        attr_kwargs: dict of attr kwargs, it's "cfg" key will be removed.

    Returns:
        {type}`AttrCfg`
    """
    cfg = attr_kwargs.pop(_CFG, None)
    if not types.is_dict(cfg):
        kwargs = {_CFG: cfg}
    else:
        kwargs = cfg
    return _AttrCfg_new(**kwargs)

def _AttrCfg_implementation(self):
    """Tells the custom transition function, if any and applicable.

    Returns:
        {type}`callable | None` the custom transition function to use, if
        any, or `None` if a different config mode is being used.
    """
    return self._state[_ATTR_CFG_VALUE] if self._state[_ATTR_CFG_WHICH] == _WhichCfg.IMPL else None

def _AttrCfg_none(self):
    """Tells if none cfg (`config.none()`) is set.

    Returns:
        {type}`bool` True if none cfg is set, False if not.
    """
    return self._state[_ATTR_CFG_VALUE] if self._state[_ATTR_CFG_WHICH] == _WhichCfg.NONE else False

def _AttrCfg_target(self):
    """Tells if target cfg is set.

    Returns:
        {type}`bool` True if target cfg is set, False if not.
    """
    return self._state[_ATTR_CFG_VALUE] if self._state[_ATTR_CFG_WHICH] == _WhichCfg.TARGET else False

def _AttrCfg_exec_group(self):
    """Tells the exec group to use if an exec transition is being used.

    Args:
        self: implicitly added.

    Returns:
        {type}`str | None` the name of the exec group to use if any,
        or `None` if `which_cfg` isn't `exec`
    """
    return self._state[_ATTR_CFG_VALUE] if self._state[_ATTR_CFG_WHICH] == _WhichCfg.EXEC else None

def _AttrCfg_set_implementation(self, impl):
    """Sets a custom transition function to use.

    Args:
        self: implicitly added.
        impl: {type}`callable` a transition implementation function.
    """
    self._state[_ATTR_CFG_WHICH] = _WhichCfg.IMPL
    self._state[_ATTR_CFG_VALUE] = impl

def _AttrCfg_set_none(self):
    """Sets to use the "none" transition."""
    self._state[_ATTR_CFG_WHICH] = _WhichCfg.NONE
    self._state[_ATTR_CFG_VALUE] = True

def _AttrCfg_set_exec(self, exec_group = None):
    """Sets to use an exec transition.

    Args:
        self: implicitly added.
        exec_group: {type}`str | None` the exec group name to use, if any.
    """
    self._state[_ATTR_CFG_WHICH] = _WhichCfg.EXEC
    self._state[_ATTR_CFG_VALUE] = exec_group

def _AttrCfg_set_target(self):
    """Sets to use the target transition."""
    self._state[_ATTR_CFG_WHICH] = _WhichCfg.TARGET
    self._state[_ATTR_CFG_VALUE] = True

def _AttrCfg_build(self):
    which = self._state[_ATTR_CFG_WHICH]
    value = self._state[_ATTR_CFG_VALUE]
    if which == None:
        return None
    elif which == _WhichCfg.TARGET:
        # config.target is Bazel 8+
        if hasattr(config, "target"):
            return config.target()
        else:
            return "target"
    elif which == _WhichCfg.EXEC:
        return config.exec(value)
    elif which == _WhichCfg.NONE:
        return config.none()
    elif types.is_function(value):
        return transition(
            implementation = value,
            # Transitions only accept unique lists of strings.
            inputs = {str(v): None for v in self._state[_INPUTS]}.keys(),
            outputs = {str(v): None for v in self._state[_OUTPUTS]}.keys(),
        )
    else:
        # Otherwise, just assume the value is valid and whoever set it knows
        # what they're doing.
        return value

# buildifier: disable=name-conventions
AttrCfg = struct(
    TYPEDEF = _AttrCfg_typedef,
    new = _AttrCfg_new,
    # keep sorted
    exec_group = _AttrCfg_exec_group,
    implementation = _AttrCfg_implementation,
    none = _AttrCfg_none,
    set_exec = _AttrCfg_set_exec,
    set_implementation = _AttrCfg_set_implementation,
    set_none = _AttrCfg_set_none,
    set_target = _AttrCfg_set_target,
    target = _AttrCfg_target,
)

def _Bool_typedef():
    """Builder for attr.bool.

    :::{function} build() -> attr.bool
    :::

    :::{function} default() -> bool.
    :::

    :::{function} doc() -> str
    :::

    :::{include} /_includes/field_kwargs_doc.md
    :::

    :::{function} mandatory() -> bool
    :::

    :::{function} set_default(v: bool)
    :::

    :::{function} set_doc(v: str)
    :::

    :::{function} set_mandatory(v: bool)
    :::

    """

def _Bool_new(**kwargs):
    """Creates a builder for `attr.bool`.

    Args:
        **kwargs: Same kwargs as {obj}`attr.bool`

    Returns:
        {type}`Bool`
    """
    kwargs_set_default_ignore_none(kwargs, _DEFAULT, False)
    kwargs_set_default_doc(kwargs)
    kwargs_set_default_mandatory(kwargs)

    # buildifier: disable=uninitialized
    self = struct(
        # keep sorted
        build = lambda: attr.bool(**self.kwargs),
        default = kwargs_getter(kwargs, _DEFAULT),
        doc = kwargs_getter_doc(kwargs),
        kwargs = kwargs,
        mandatory = kwargs_getter_mandatory(kwargs),
        set_default = kwargs_setter(kwargs, _DEFAULT),
        set_doc = kwargs_setter_doc(kwargs),
        set_mandatory = kwargs_setter_mandatory(kwargs),
    )
    return self

# buildifier: disable=name-conventions
Bool = struct(
    TYPEDEF = _Bool_typedef,
    new = _Bool_new,
)

def _Int_typedef():
    """Builder for attr.int.

    :::{function} build() -> attr.int
    :::

    :::{function} default() -> int
    :::

    :::{function} doc() -> str
    :::

    :::{include} /_includes/field_kwargs_doc.md
    :::

    :::{function} mandatory() -> bool
    :::

    :::{function} values() -> list[int]

    The returned value is a mutable reference to the underlying list.
    :::

    :::{function} set_default(v: int)
    :::

    :::{function} set_doc(v: str)
    :::

    :::{function} set_mandatory(v: bool)
    :::
    """

def _Int_new(**kwargs):
    """Creates a builder for `attr.int`.

    Args:
        **kwargs: Same kwargs as {obj}`attr.int`

    Returns:
        {type}`Int`
    """
    kwargs_set_default_ignore_none(kwargs, _DEFAULT, 0)
    kwargs_set_default_doc(kwargs)
    kwargs_set_default_mandatory(kwargs)
    kwargs_set_default_list(kwargs, _VALUES)

    # buildifier: disable=uninitialized
    self = struct(
        build = lambda: attr.int(**self.kwargs),
        default = kwargs_getter(kwargs, _DEFAULT),
        doc = kwargs_getter_doc(kwargs),
        kwargs = kwargs,
        mandatory = kwargs_getter_mandatory(kwargs),
        values = kwargs_getter(kwargs, _VALUES),
        set_default = kwargs_setter(kwargs, _DEFAULT),
        set_doc = kwargs_setter_doc(kwargs),
        set_mandatory = kwargs_setter_mandatory(kwargs),
    )
    return self

# buildifier: disable=name-conventions
Int = struct(
    TYPEDEF = _Int_typedef,
    new = _Int_new,
)

def _IntList_typedef():
    """Builder for attr.int_list.

    :::{function} allow_empty() -> bool
    :::

    :::{function} build() -> attr.int_list
    :::

    :::{function} default() -> list[int]
    :::

    :::{function} doc() -> str
    :::

    :::{include} /_includes/field_kwargs_doc.md
    :::

    :::{function} mandatory() -> bool
    :::

    :::{function} set_allow_empty(v: bool)
    :::

    :::{function} set_doc(v: str)
    :::

    :::{function} set_mandatory(v: bool)
    :::
    """

def _IntList_new(**kwargs):
    """Creates a builder for `attr.int_list`.

    Args:
        **kwargs: Same as {obj}`attr.int_list`.

    Returns:
        {type}`IntList`
    """
    kwargs_set_default_list(kwargs, _DEFAULT)
    kwargs_set_default_doc(kwargs)
    kwargs_set_default_mandatory(kwargs)
    _kwargs_set_default_allow_empty(kwargs)

    # buildifier: disable=uninitialized
    self = struct(
        # keep sorted
        allow_empty = _kwargs_getter_allow_empty(kwargs),
        build = lambda: attr.int_list(**self.kwargs),
        default = kwargs_getter(kwargs, _DEFAULT),
        doc = kwargs_getter_doc(kwargs),
        kwargs = kwargs,
        mandatory = kwargs_getter_mandatory(kwargs),
        set_allow_empty = _kwargs_setter_allow_empty(kwargs),
        set_doc = kwargs_setter_doc(kwargs),
        set_mandatory = kwargs_setter_mandatory(kwargs),
    )
    return self

# buildifier: disable=name-conventions
IntList = struct(
    TYPEDEF = _IntList_typedef,
    new = _IntList_new,
)

def _Label_typedef():
    """Builder for `attr.label` objects.

    :::{function} allow_files() -> bool | list[str] | None

    Note that `allow_files` is mutually exclusive with `allow_single_file`.
    Only one of the two can have a value set.
    :::

    :::{function} allow_single_file() -> bool | None
    Note that `allow_single_file` is mutually exclusive with `allow_files`.
    Only one of the two can have a value set.
    :::

    :::{function} aspects() -> list[aspect]

    The returned list is a mutable reference to the underlying list.
    :::

    :::{function} build() -> attr.label
    :::

    :::{field} cfg
    :type: AttrCfg
    :::

    :::{function} default() -> str | label | configuration_field | None
    :::

    :::{function} doc() -> str
    :::

    :::{function} executable() -> bool
    :::

    :::{include} /_includes/field_kwargs_doc.md
    :::

    :::{function} mandatory() -> bool
    :::


    :::{function} providers() -> list[list[provider]]
    The returned list is a mutable reference to the underlying list.
    :::

    :::{function} set_default(v: str | Label)
    :::

    :::{function} set_doc(v: str)
    :::

    :::{function} set_executable(v: bool)
    :::

    :::{function} set_mandatory(v: bool)
    :::
    """

def _Label_new(**kwargs):
    """Creates a builder for `attr.label`.

    Args:
        **kwargs: The same as {obj}`attr.label()`.

    Returns:
        {type}`Label`
    """
    kwargs_set_default_ignore_none(kwargs, "executable", False)
    _kwargs_set_default_aspects(kwargs)
    _kwargs_set_default_providers(kwargs)
    kwargs_set_default_doc(kwargs)
    kwargs_set_default_mandatory(kwargs)

    kwargs[_DEFAULT] = to_label_maybe(kwargs.get(_DEFAULT))

    # buildifier: disable=uninitialized
    self = struct(
        # keep sorted
        add_allow_files = lambda v: _Label_add_allow_files(self, v),
        allow_files = _kwargs_getter_allow_files(kwargs),
        allow_single_file = kwargs_getter(kwargs, _ALLOW_SINGLE_FILE),
        aspects = _kwargs_getter_aspects(kwargs),
        build = lambda: _common_label_build(self, attr.label),
        cfg = _AttrCfg_from_attr_kwargs_pop(kwargs),
        default = kwargs_getter(kwargs, _DEFAULT),
        doc = kwargs_getter_doc(kwargs),
        executable = kwargs_getter(kwargs, "executable"),
        kwargs = kwargs,
        mandatory = kwargs_getter_mandatory(kwargs),
        providers = _kwargs_getter_providers(kwargs),
        set_allow_files = lambda v: _Label_set_allow_files(self, v),
        set_allow_single_file = lambda v: _Label_set_allow_single_file(self, v),
        set_default = kwargs_setter(kwargs, _DEFAULT),
        set_doc = kwargs_setter_doc(kwargs),
        set_executable = kwargs_setter(kwargs, "executable"),
        set_mandatory = kwargs_setter_mandatory(kwargs),
    )
    return self

def _Label_set_allow_files(self, v):
    """Set the allow_files arg

    NOTE: Setting `allow_files` unsets `allow_single_file`

    Args:
        self: implicitly added.
        v: {type}`bool | list[str] | None` the value to set to.
            If set to `None`, then `allow_files` is unset.
    """
    if v == None:
        self.kwargs.pop(_ALLOW_FILES, None)
    else:
        self.kwargs[_ALLOW_FILES] = v
        self.kwargs.pop(_ALLOW_SINGLE_FILE, None)

def _Label_add_allow_files(self, *values):
    """Adds allowed file extensions

    NOTE: Add an allowed file extension unsets `allow_single_file`

    Args:
        self: implicitly added.
        *values: {type}`str` file extensions to allow (including dot)
    """
    self.kwargs.pop(_ALLOW_SINGLE_FILE, None)
    if not types.is_list(self.kwargs.get(_ALLOW_FILES)):
        self.kwargs[_ALLOW_FILES] = []
    existing = self.kwargs[_ALLOW_FILES]
    existing.extend([v for v in values if v not in existing])

def _Label_set_allow_single_file(self, v):
    """Sets the allow_single_file arg.

    NOTE: Setting `allow_single_file` unsets `allow_file`

    Args:
        self: implicitly added.
        v: {type}`bool | None` the value to set to.
            If set to `None`, then `allow_single_file` is unset.
    """
    if v == None:
        self.kwargs.pop(_ALLOW_SINGLE_FILE, None)
    else:
        self.kwargs[_ALLOW_SINGLE_FILE] = v
        self.kwargs.pop(_ALLOW_FILES, None)

# buildifier: disable=name-conventions
Label = struct(
    TYPEDEF = _Label_typedef,
    new = _Label_new,
    set_allow_files = _Label_set_allow_files,
    add_allow_files = _Label_add_allow_files,
    set_allow_single_file = _Label_set_allow_single_file,
)

def _LabelKeyedStringDict_typedef():
    """Builder for attr.label_keyed_string_dict.

    :::{function} aspects() -> list[aspect]
    The returned list is a mutable reference to the underlying list.
    :::

    :::{function} allow_files() -> bool | list[str]
    :::

    :::{function} allow_empty() -> bool
    :::

    :::{field} cfg
    :type: AttrCfg
    :::

    :::{function} default() -> dict[str | Label, str] | callable
    :::

    :::{function} doc() -> str
    :::

    :::{include} /_includes/field_kwargs_doc.md
    :::

    :::{function} mandatory() -> bool
    :::

    :::{function} providers() -> list[provider | list[provider]]

    Returns a mutable reference to the underlying list.
    :::

    :::{function} set_mandatory(v: bool)
    :::
    :::{function} set_allow_empty(v: bool)
    :::
    :::{function} set_default(v: dict[str | Label, str] | callable)
    :::
    :::{function} set_doc(v: str)
    :::
    :::{function} set_allow_files(v: bool | list[str])
    :::
    """

def _LabelKeyedStringDict_new(**kwargs):
    """Creates a builder for `attr.label_keyed_string_dict`.

    Args:
        **kwargs: Same as {obj}`attr.label_keyed_string_dict`.

    Returns:
        {type}`LabelKeyedStringDict`
    """
    kwargs_set_default_ignore_none(kwargs, _DEFAULT, {})
    _kwargs_set_default_aspects(kwargs)
    _kwargs_set_default_providers(kwargs)
    _kwargs_set_default_allow_empty(kwargs)
    _kwargs_set_default_allow_files(kwargs)
    kwargs_set_default_doc(kwargs)
    kwargs_set_default_mandatory(kwargs)

    # buildifier: disable=uninitialized
    self = struct(
        # keep sorted
        add_allow_files = lambda *v: _LabelKeyedStringDict_add_allow_files(self, *v),
        allow_empty = _kwargs_getter_allow_empty(kwargs),
        allow_files = _kwargs_getter_allow_files(kwargs),
        aspects = _kwargs_getter_aspects(kwargs),
        build = lambda: _common_label_build(self, attr.label_keyed_string_dict),
        cfg = _AttrCfg_from_attr_kwargs_pop(kwargs),
        default = kwargs_getter(kwargs, _DEFAULT),
        doc = kwargs_getter_doc(kwargs),
        kwargs = kwargs,
        mandatory = kwargs_getter_mandatory(kwargs),
        providers = _kwargs_getter_providers(kwargs),
        set_allow_empty = _kwargs_setter_allow_empty(kwargs),
        set_allow_files = _kwargs_setter_allow_files(kwargs),
        set_default = kwargs_setter(kwargs, _DEFAULT),
        set_doc = kwargs_setter_doc(kwargs),
        set_mandatory = kwargs_setter_mandatory(kwargs),
    )
    return self

def _LabelKeyedStringDict_add_allow_files(self, *values):
    """Adds allowed file extensions

    Args:
        self: implicitly added.
        *values: {type}`str` file extensions to allow (including dot)
    """
    if not types.is_list(self.kwargs.get(_ALLOW_FILES)):
        self.kwargs[_ALLOW_FILES] = []
    existing = self.kwargs[_ALLOW_FILES]
    existing.extend([v for v in values if v not in existing])

# buildifier: disable=name-conventions
LabelKeyedStringDict = struct(
    TYPEDEF = _LabelKeyedStringDict_typedef,
    new = _LabelKeyedStringDict_new,
    add_allow_files = _LabelKeyedStringDict_add_allow_files,
)

def _LabelList_typedef():
    """Builder for `attr.label_list`

    :::{function} aspects() -> list[aspect]
    :::

    :::{function} allow_files() -> bool | list[str]
    :::

    :::{function} allow_empty() -> bool
    :::

    :::{function} build() -> attr.label_list
    :::

    :::{field} cfg
    :type: AttrCfg
    :::

    :::{function} default() -> list[str|Label] | configuration_field | callable
    :::

    :::{function} doc() -> str
    :::

    :::{include} /_includes/field_kwargs_doc.md
    :::

    :::{function} mandatory() -> bool
    :::

    :::{function} providers() -> list[provider | list[provider]]
    :::

    :::{function} set_allow_empty(v: bool)
    :::

    :::{function} set_allow_files(v: bool | list[str])
    :::

    :::{function} set_default(v: list[str|Label] | configuration_field | callable)
    :::

    :::{function} set_doc(v: str)
    :::

    :::{function} set_mandatory(v: bool)
    :::
    """

def _LabelList_new(**kwargs):
    """Creates a builder for `attr.label_list`.

    Args:
        **kwargs: Same as {obj}`attr.label_list`.

    Returns:
        {type}`LabelList`
    """
    _kwargs_set_default_allow_empty(kwargs)
    kwargs_set_default_mandatory(kwargs)
    kwargs_set_default_doc(kwargs)
    if kwargs.get(_ALLOW_FILES) == None:
        kwargs[_ALLOW_FILES] = False
    _kwargs_set_default_aspects(kwargs)
    kwargs_set_default_list(kwargs, _DEFAULT)
    _kwargs_set_default_providers(kwargs)

    # buildifier: disable=uninitialized
    self = struct(
        # keep sorted
        allow_empty = _kwargs_getter_allow_empty(kwargs),
        allow_files = _kwargs_getter_allow_files(kwargs),
        aspects = _kwargs_getter_aspects(kwargs),
        build = lambda: _common_label_build(self, attr.label_list),
        cfg = _AttrCfg_from_attr_kwargs_pop(kwargs),
        default = kwargs_getter(kwargs, _DEFAULT),
        doc = kwargs_getter_doc(kwargs),
        kwargs = kwargs,
        mandatory = kwargs_getter_mandatory(kwargs),
        providers = _kwargs_getter_providers(kwargs),
        set_allow_empty = _kwargs_setter_allow_empty(kwargs),
        set_allow_files = _kwargs_setter_allow_files(kwargs),
        set_default = kwargs_setter(kwargs, _DEFAULT),
        set_doc = kwargs_setter_doc(kwargs),
        set_mandatory = kwargs_setter_mandatory(kwargs),
    )
    return self

# buildifier: disable=name-conventions
LabelList = struct(
    TYPEDEF = _LabelList_typedef,
    new = _LabelList_new,
)

def _Output_typedef():
    """Builder for attr.output

    :::{function} build() -> attr.output
    :::

    :::{function} doc() -> str
    :::

    :::{include} /_includes/field_kwargs_doc.md
    :::

    :::{function} mandatory() -> bool
    :::

    :::{function} set_doc(v: str)
    :::

    :::{function} set_mandatory(v: bool)
    :::
    """

def _Output_new(**kwargs):
    """Creates a builder for `attr.output`.

    Args:
        **kwargs: Same as {obj}`attr.output`.

    Returns:
        {type}`Output`
    """
    kwargs_set_default_doc(kwargs)
    kwargs_set_default_mandatory(kwargs)

    # buildifier: disable=uninitialized
    self = struct(
        # keep sorted
        build = lambda: attr.output(**self.kwargs),
        doc = kwargs_getter_doc(kwargs),
        kwargs = kwargs,
        mandatory = kwargs_getter_mandatory(kwargs),
        set_doc = kwargs_setter_doc(kwargs),
        set_mandatory = kwargs_setter_mandatory(kwargs),
    )
    return self

# buildifier: disable=name-conventions
Output = struct(
    TYPEDEF = _Output_typedef,
    new = _Output_new,
)

def _OutputList_typedef():
    """Builder for attr.output_list

    :::{function} allow_empty() -> bool
    :::

    :::{function} build() -> attr.output
    :::

    :::{function} doc() -> str
    :::

    :::{include} /_includes/field_kwargs_doc.md
    :::

    :::{function} mandatory() -> bool
    :::

    :::{function} set_allow_empty(v: bool)
    :::
    :::{function} set_doc(v: str)
    :::
    :::{function} set_mandatory(v: bool)
    :::
    """

def _OutputList_new(**kwargs):
    """Creates a builder for `attr.output_list`.

    Args:
        **kwargs: Same as {obj}`attr.output_list`.

    Returns:
        {type}`OutputList`
    """
    kwargs_set_default_doc(kwargs)
    kwargs_set_default_mandatory(kwargs)
    _kwargs_set_default_allow_empty(kwargs)

    # buildifier: disable=uninitialized
    self = struct(
        allow_empty = _kwargs_getter_allow_empty(kwargs),
        build = lambda: attr.output_list(**self.kwargs),
        doc = kwargs_getter_doc(kwargs),
        kwargs = kwargs,
        mandatory = kwargs_getter_mandatory(kwargs),
        set_allow_empty = _kwargs_setter_allow_empty(kwargs),
        set_doc = kwargs_setter_doc(kwargs),
        set_mandatory = kwargs_setter_mandatory(kwargs),
    )
    return self

# buildifier: disable=name-conventions
OutputList = struct(
    TYPEDEF = _OutputList_typedef,
    new = _OutputList_new,
)

def _String_typedef():
    """Builder for `attr.string`

    :::{function} build() -> attr.string
    :::

    :::{function} default() -> str | configuration_field
    :::

    :::{function} doc() -> str
    :::

    :::{include} /_includes/field_kwargs_doc.md
    :::

    :::{function} mandatory() -> bool
    :::

    :::{function} values() -> list[str]
    :::

    :::{function} set_default(v: str | configuration_field)
    :::

    :::{function} set_doc(v: str)
    :::

    :::{function} set_mandatory(v: bool)
    :::
    """

def _String_new(**kwargs):
    """Creates a builder for `attr.string`.

    Args:
        **kwargs: Same as {obj}`attr.string`.

    Returns:
        {type}`String`
    """
    kwargs_set_default_ignore_none(kwargs, _DEFAULT, "")
    kwargs_set_default_list(kwargs, _VALUES)
    kwargs_set_default_doc(kwargs)
    kwargs_set_default_mandatory(kwargs)

    # buildifier: disable=uninitialized
    self = struct(
        default = kwargs_getter(kwargs, _DEFAULT),
        doc = kwargs_getter_doc(kwargs),
        mandatory = kwargs_getter_mandatory(kwargs),
        build = lambda: attr.string(**self.kwargs),
        kwargs = kwargs,
        values = kwargs_getter(kwargs, _VALUES),
        set_default = kwargs_setter(kwargs, _DEFAULT),
        set_doc = kwargs_setter_doc(kwargs),
        set_mandatory = kwargs_setter_mandatory(kwargs),
    )
    return self

# buildifier: disable=name-conventions
String = struct(
    TYPEDEF = _String_typedef,
    new = _String_new,
)

def _StringDict_typedef():
    """Builder for `attr.string_dict`

    :::{function} default() -> dict[str, str]
    :::

    :::{function} doc() -> str
    :::

    :::{function} mandatory() -> bool
    :::

    :::{function} allow_empty() -> bool
    :::

    :::{function} build() -> attr.string_dict
    :::

    :::{include} /_includes/field_kwargs_doc.md
    :::

    :::{function} set_doc(v: str)
    :::
    :::{function} set_mandatory(v: bool)
    :::
    :::{function} set_allow_empty(v: bool)
    :::
    """

def _StringDict_new(**kwargs):
    """Creates a builder for `attr.string_dict`.

    Args:
        **kwargs: The same args as for `attr.string_dict`.

    Returns:
        {type}`StringDict`
    """
    kwargs_set_default_ignore_none(kwargs, _DEFAULT, {})
    kwargs_set_default_doc(kwargs)
    kwargs_set_default_mandatory(kwargs)
    _kwargs_set_default_allow_empty(kwargs)

    # buildifier: disable=uninitialized
    self = struct(
        allow_empty = _kwargs_getter_allow_empty(kwargs),
        build = lambda: attr.string_dict(**self.kwargs),
        default = kwargs_getter(kwargs, _DEFAULT),
        doc = kwargs_getter_doc(kwargs),
        kwargs = kwargs,
        mandatory = kwargs_getter_mandatory(kwargs),
        set_allow_empty = _kwargs_setter_allow_empty(kwargs),
        set_doc = kwargs_setter_doc(kwargs),
        set_mandatory = kwargs_setter_mandatory(kwargs),
    )
    return self

# buildifier: disable=name-conventions
StringDict = struct(
    TYPEDEF = _StringDict_typedef,
    new = _StringDict_new,
)

def _StringKeyedLabelDict_typedef():
    """Builder for attr.string_keyed_label_dict.

    :::{function} allow_empty() -> bool
    :::

    :::{function} allow_files() -> bool | list[str]
    :::

    :::{function} aspects() -> list[aspect]
    :::

    :::{function} build() -> attr.string_list
    :::

    :::{field} cfg
    :type: AttrCfg
    :::

    :::{function} default() -> dict[str, Label] | callable
    :::

    :::{function} doc() -> str
    :::

    :::{function} mandatory() -> bool
    :::

    :::{function} providers() -> list[list[provider]]
    :::

    :::{include} /_includes/field_kwargs_doc.md
    :::

    :::{function} set_allow_empty(v: bool)
    :::

    :::{function} set_allow_files(v: bool | list[str])
    :::

    :::{function} set_doc(v: str)
    :::

    :::{function} set_default(v: dict[str, Label] | callable)
    :::

    :::{function} set_mandatory(v: bool)
    :::
    """

def _StringKeyedLabelDict_new(**kwargs):
    """Creates a builder for `attr.string_keyed_label_dict`.

    Args:
        **kwargs: Same as {obj}`attr.string_keyed_label_dict`.

    Returns:
        {type}`StringKeyedLabelDict`
    """
    kwargs_set_default_ignore_none(kwargs, _DEFAULT, {})
    kwargs_set_default_doc(kwargs)
    kwargs_set_default_mandatory(kwargs)
    _kwargs_set_default_allow_files(kwargs)
    _kwargs_set_default_allow_empty(kwargs)
    _kwargs_set_default_aspects(kwargs)
    _kwargs_set_default_providers(kwargs)

    # buildifier: disable=uninitialized
    self = struct(
        allow_empty = _kwargs_getter_allow_empty(kwargs),
        allow_files = _kwargs_getter_allow_files(kwargs),
        build = lambda: _common_label_build(self, attr.string_keyed_label_dict),
        cfg = _AttrCfg_from_attr_kwargs_pop(kwargs),
        default = kwargs_getter(kwargs, _DEFAULT),
        doc = kwargs_getter_doc(kwargs),
        kwargs = kwargs,
        mandatory = kwargs_getter_mandatory(kwargs),
        set_allow_empty = _kwargs_setter_allow_empty(kwargs),
        set_allow_files = _kwargs_setter_allow_files(kwargs),
        set_default = kwargs_setter(kwargs, _DEFAULT),
        set_doc = kwargs_setter_doc(kwargs),
        set_mandatory = kwargs_setter_mandatory(kwargs),
        providers = _kwargs_getter_providers(kwargs),
        aspects = _kwargs_getter_aspects(kwargs),
    )
    return self

# buildifier: disable=name-conventions
StringKeyedLabelDict = struct(
    TYPEDEF = _StringKeyedLabelDict_typedef,
    new = _StringKeyedLabelDict_new,
)

def _StringList_typedef():
    """Builder for `attr.string_list`

    :::{function} allow_empty() -> bool
    :::

    :::{function} build() -> attr.string_list
    :::

    :::{field} default
    :type: list[str] | configuration_field
    :::

    :::{function} doc() -> str
    :::

    :::{function} mandatory() -> bool
    :::

    :::{include} /_includes/field_kwargs_doc.md
    :::

    :::{function} set_allow_empty(v: bool)
    :::

    :::{function} set_default(v: list[str] | configuration_field)
    :::

    :::{function} set_doc(v: str)
    :::

    :::{function} set_mandatory(v: bool)
    :::
    """

def _StringList_new(**kwargs):
    """Creates a builder for `attr.string_list`.

    Args:
        **kwargs: Same as {obj}`attr.string_list`.

    Returns:
        {type}`StringList`
    """
    kwargs_set_default_ignore_none(kwargs, _DEFAULT, [])
    kwargs_set_default_doc(kwargs)
    kwargs_set_default_mandatory(kwargs)
    _kwargs_set_default_allow_empty(kwargs)

    # buildifier: disable=uninitialized
    self = struct(
        allow_empty = _kwargs_getter_allow_empty(kwargs),
        build = lambda: attr.string_list(**self.kwargs),
        default = kwargs_getter(kwargs, _DEFAULT),
        doc = kwargs_getter_doc(kwargs),
        kwargs = kwargs,
        mandatory = kwargs_getter_mandatory(kwargs),
        set_allow_empty = _kwargs_setter_allow_empty(kwargs),
        set_default = kwargs_setter(kwargs, _DEFAULT),
        set_doc = kwargs_setter_doc(kwargs),
        set_mandatory = kwargs_setter_mandatory(kwargs),
    )
    return self

# buildifier: disable=name-conventions
StringList = struct(
    TYPEDEF = _StringList_typedef,
    new = _StringList_new,
)

def _StringListDict_typedef():
    """Builder for attr.string_list_dict.

    :::{function} allow_empty() -> bool
    :::

    :::{function} build() -> attr.string_list
    :::

    :::{function} default() -> dict[str, list[str]]
    :::

    :::{function} doc() -> str
    :::

    :::{function} mandatory() -> bool
    :::

    :::{include} /_includes/field_kwargs_doc.md
    :::

    :::{function} set_allow_empty(v: bool)
    :::

    :::{function} set_doc(v: str)
    :::

    :::{function} set_mandatory(v: bool)
    :::
    """

def _StringListDict_new(**kwargs):
    """Creates a builder for `attr.string_list_dict`.

    Args:
        **kwargs: Same as {obj}`attr.string_list_dict`.

    Returns:
        {type}`StringListDict`
    """
    kwargs_set_default_ignore_none(kwargs, _DEFAULT, {})
    kwargs_set_default_doc(kwargs)
    kwargs_set_default_mandatory(kwargs)
    _kwargs_set_default_allow_empty(kwargs)

    # buildifier: disable=uninitialized
    self = struct(
        allow_empty = _kwargs_getter_allow_empty(kwargs),
        build = lambda: attr.string_list_dict(**self.kwargs),
        default = kwargs_getter(kwargs, _DEFAULT),
        doc = kwargs_getter_doc(kwargs),
        kwargs = kwargs,
        mandatory = kwargs_getter_mandatory(kwargs),
        set_allow_empty = _kwargs_setter_allow_empty(kwargs),
        set_default = kwargs_setter(kwargs, _DEFAULT),
        set_doc = kwargs_setter_doc(kwargs),
        set_mandatory = kwargs_setter_mandatory(kwargs),
    )
    return self

# buildifier: disable=name-conventions
StringListDict = struct(
    TYPEDEF = _StringListDict_typedef,
    new = _StringListDict_new,
)

attrb = struct(
    # keep sorted
    Bool = _Bool_new,
    Int = _Int_new,
    IntList = _IntList_new,
    Label = _Label_new,
    LabelKeyedStringDict = _LabelKeyedStringDict_new,
    LabelList = _LabelList_new,
    Output = _Output_new,
    OutputList = _OutputList_new,
    String = _String_new,
    StringDict = _StringDict_new,
    StringKeyedLabelDict = _StringKeyedLabelDict_new,
    StringList = _StringList_new,
    StringListDict = _StringListDict_new,
    WhichCfg = _WhichCfg,
)
