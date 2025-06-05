# Copyright 2023 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Text manipulation utilities useful for repository rule writing."""

def _indent(text, indent = " " * 4):
    if "\n" not in text:
        return indent + text

    return "\n".join([indent + line for line in text.splitlines()])

def _hanging_indent(text, indent = " " * 4):
    if "\n" not in text:
        return text

    lines = text.splitlines()
    for i, line in enumerate(lines):
        lines[i] = (indent if i != 0 else "") + line
    return "\n".join(lines)

def _render_alias(name, actual, *, visibility = None):
    args = [
        "name = \"{}\",".format(name),
        "actual = {},".format(actual),
    ]

    if visibility:
        args.append("visibility = {},".format(render.list(visibility)))

    return "\n".join([
        "alias(",
    ] + [_indent(arg) for arg in args] + [
        ")",
    ])

def _render_dict(d, *, key_repr = repr, value_repr = repr):
    if not d:
        return "{}"

    return "\n".join([
        "{",
        _indent("\n".join([
            "{}: {},".format(key_repr(k), value_repr(v))
            for k, v in d.items()
        ])),
        "}",
    ])

def _render_select(selects, *, no_match_error = None, key_repr = repr, value_repr = repr, name = "select"):
    dict_str = _render_dict(selects, key_repr = key_repr, value_repr = value_repr) + ","

    if no_match_error:
        args = "\n".join([
            "",
            _indent(dict_str),
            _indent("no_match_error = {},".format(no_match_error)),
            "",
        ])
    else:
        args = "\n".join([
            "",
            _indent(dict_str),
            "",
        ])

    return "{}({})".format(name, args)

def _render_list(items, *, hanging_indent = ""):
    """Convert a list to formatted text.

    Args:
        items: list of items.
        hanging_indent: str, indent to apply to second and following lines of
            the formatted text.

    Returns:
        The list pretty formatted as a string.
    """
    if not items:
        return "[]"

    if len(items) == 1:
        return "[{}]".format(repr(items[0]))

    text = "\n".join([
        "[",
        _indent("\n".join([
            "{},".format(repr(item))
            for item in items
        ])),
        "]",
    ])
    if hanging_indent:
        text = _hanging_indent(text, hanging_indent)
    return text

def _render_str(value):
    return repr(value)

def _render_tuple(items, *, value_repr = repr):
    if not items:
        return "tuple()"

    if len(items) == 1:
        return "({},)".format(value_repr(items[0]))

    return "\n".join([
        "(",
        _indent("\n".join([
            "{},".format(value_repr(item))
            for item in items
        ])),
        ")",
    ])

def _render_kwargs(items, *, value_repr = repr):
    if not items:
        return ""

    return "\n".join([
        "{} = {},".format(k, value_repr(v)).lstrip()
        for k, v in items.items()
    ])

def _render_call(fn_name, **kwargs):
    if not kwargs:
        return fn_name + "()"

    return "{}(\n{}\n)".format(fn_name, _indent(_render_kwargs(kwargs, value_repr = lambda x: x)))

def _toolchain_prefix(index, name, pad_length):
    """Prefixes the given name with the index, padded with zeros to ensure lexicographic sorting.

    Examples:
      toolchain_prefix(   2, "foo", 4) == "_0002_foo_"
      toolchain_prefix(2000, "foo", 4) == "_2000_foo_"
    """
    return "_{}_{}_".format(_left_pad_zero(index, pad_length), name)

def _left_pad_zero(index, length):
    if index < 0:
        fail("index must be non-negative")
    return ("0" * length + str(index))[-length:]

render = struct(
    alias = _render_alias,
    dict = _render_dict,
    call = _render_call,
    hanging_indent = _hanging_indent,
    indent = _indent,
    kwargs = _render_kwargs,
    left_pad_zero = _left_pad_zero,
    list = _render_list,
    select = _render_select,
    str = _render_str,
    toolchain_prefix = _toolchain_prefix,
    tuple = _render_tuple,
)
