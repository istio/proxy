# Copyright 2025 The Bazel Authors. All rights reserved.
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

"""This module is for implementing PEP508 in starlark as FeatureFlagInfo
"""

load("//python/private:enum.bzl", "enum")
load("//python/private:version.bzl", "version")

# The expression parsing and resolution for the PEP508 is below
#

_STATE = enum(
    STRING = "string",
    VAR = "var",
    OP = "op",
    NONE = "none",
)
_BRACKETS = "()"
_OPCHARS = "<>!=~"
_QUOTES = "'\""
_WSP = " \t"
_NON_VERSION_VAR_NAMES = [
    "implementation_name",
    "os_name",
    "platform_machine",
    "platform_python_implementation",
    "platform_release",
    "platform_system",
    "sys_platform",
    "extra",
]
_AND = "and"
_OR = "or"
_NOT = "not"
_ENV_ALIASES = "_aliases"

def tokenize(marker):
    """Tokenize the input string.

    The output will have double-quoted values (i.e. the quoting will be normalized) and all of the whitespace will be trimmed.

    Args:
        marker: {type}`str` The input to tokenize.

    Returns:
        The {type}`str` that is the list of recognized tokens that should be parsed.
    """
    if not marker:
        return []

    tokens = []
    token = ""
    state = _STATE.NONE
    char = ""

    # Due to the `continue` in the loop, we will be processing chars at a slower pace
    for _ in range(2 * len(marker)):
        if token and (state == _STATE.NONE or not marker):
            if tokens and token == "in" and tokens[-1] == _NOT:
                tokens[-1] += " " + token
            else:
                tokens.append(token)
            token = ""

        if not marker:
            return tokens

        char = marker[0]
        if char in _BRACKETS:
            state = _STATE.NONE
            token = char
        elif state == _STATE.STRING and char in _QUOTES:
            state = _STATE.NONE
            token = '"{}"'.format(token)
        elif (
            (state == _STATE.VAR and not char.isalnum() and char != "_") or
            (state == _STATE.OP and char not in _OPCHARS)
        ):
            state = _STATE.NONE
            continue  # Skip consuming the char below
        elif state == _STATE.NONE:
            # Transition from _STATE.NONE to something or stay in NONE
            if char in _QUOTES:
                state = _STATE.STRING
            elif char.isalnum():
                state = _STATE.VAR
                token += char
            elif char in _OPCHARS:
                state = _STATE.OP
                token += char
            elif char in _WSP:
                state = _STATE.NONE
            else:
                fail("BUG: Cannot parse '{}' in {} ({})".format(char, state, marker))
        else:
            token += char

        # Consume the char
        marker = marker[1:]

    return fail("BUG: failed to process the marker in allocated cycles: {}".format(marker))

def evaluate(marker, *, env, strict = True, **kwargs):
    """Evaluate the marker against a given env.

    Args:
        marker: {type}`str` The string marker to evaluate.
        env: {type}`dict[str, str]` The environment to evaluate the marker against.
        strict: {type}`bool` A setting to not fail on missing values in the env.
        **kwargs: Extra kwargs to be passed to the expression evaluator.

    Returns:
        The {type}`bool | str` If the marker is compatible with the given env. If strict is
        `False`, then the output type is `str` which will represent the remaining
        expression that has not been evaluated.
    """
    tokens = tokenize(marker)

    ast = _new_expr(marker = marker, **kwargs)
    for _ in range(len(tokens) * 2):
        if not tokens:
            break

        tokens = ast.parse(env = env, tokens = tokens, strict = strict)

    if not tokens:
        return ast.value()

    fail("Could not evaluate: {}".format(marker))

_STRING_REPLACEMENTS = {
    "!=": "neq",
    "(": "_",
    ")": "_",
    "<": "lt",
    "<=": "lteq",
    "==": "eq",
    "===": "eeq",
    ">": "gt",
    ">=": "gteq",
    "not in": "not_in",
    "~==": "cmp",
}

def to_string(marker):
    return "_".join([
        _STRING_REPLACEMENTS.get(t, t)
        for t in tokenize(marker)
    ]).replace("\"", "")

def _and_fn(x, y):
    """Our custom `and` evaluation function.

    Allow partial evaluation if one of the values is a string, return the
    string value because that means that `marker_expr` was set to
    `strict = False` and we are only evaluating what we can.
    """
    if not (x and y):
        return False

    x_is_str = type(x) == type("")
    y_is_str = type(y) == type("")
    if x_is_str and y_is_str:
        return "{} and {}".format(x, y)
    elif x_is_str:
        return x
    else:
        return y

def _or_fn(x, y):
    """Our custom `or` evaluation function.

    Allow partial evaluation if one of the values is a string, return the
    string value because that means that `marker_expr` was set to
    `strict = False` and we are only evaluating what we can.
    """
    x_is_str = type(x) == type("")
    y_is_str = type(y) == type("")

    if x_is_str and y_is_str:
        return "{} or {}".format(x, y) if x and y else ""
    elif x_is_str:
        return "" if y else x
    elif y_is_str:
        return "" if x else y
    else:
        return x or y

def _not_fn(x):
    """Our custom `not` evaluation function.

    Allow partial evaluation if the value is a string.
    """
    if type(x) == type(""):
        return "not {}".format(x)
    else:
        return not x

def _new_expr(
        *,
        marker,
        and_fn = _and_fn,
        or_fn = _or_fn,
        not_fn = _not_fn):
    # buildifier: disable=uninitialized
    self = struct(
        marker = marker,
        tree = [],
        parse = lambda **kwargs: _parse(self, **kwargs),
        value = lambda: _value(self),
        # This is a way for us to have a handle to the currently constructed
        # expression tree branch.
        current = lambda: self._current[-1] if self._current else None,
        _current = [],
        _and = and_fn,
        _or = or_fn,
        _not = not_fn,
    )
    return self

def _parse(self, *, env, tokens, strict = False):
    """The parse function takes the consumed tokens and returns the remaining."""
    token, remaining = tokens[0], tokens[1:]

    if token == "(":
        expr = _open_parenthesis(self)
    elif token == ")":
        expr = _close_parenthesis(self)
    elif token == _AND:
        expr = _and_expr(self)
    elif token == _OR:
        expr = _or_expr(self)
    elif token == _NOT:
        expr = _not_expr(self)
    else:
        expr = marker_expr(env = env, strict = strict, *tokens[:3])
        remaining = tokens[3:]

    _append(self, expr)
    return remaining

def _value(self):
    """Evaluate the expression tree"""
    if not self.tree:
        # Basic case where no marker should evaluate to True
        return True

    for _ in range(len(self.tree)):
        if len(self.tree) == 1:
            return self.tree[0]

        # Resolve all of the `or` expressions as it is safe to do now since all
        # `and` and `not` expressions have been taken care of by now.
        if getattr(self.tree[-2], "op", None) == _OR:
            current = self.tree.pop()
            self.tree[-1] = self.tree[-1].value(current)
        else:
            break

    fail("BUG: invalid state: {}".format(self.tree))

def marker_expr(left, op, right, *, env, strict = True):
    """Evaluate a marker expression

    Args:
        left: {type}`str` the env identifier or a value quoted in `"`.
        op: {type}`str` the operation to carry out.
        right: {type}`str` the env identifier or a value quoted in `"`.
        strict: {type}`bool` if false, only evaluates the values that are present
            in the environment, otherwise returns the original expression.
        env: {type}`dict[str, str]` the `env` to substitute `env` identifiers in
            the `<left> <op> <right>` expression. Note, if `env` has a key
            "_aliases", then we will do normalization so that we can ensure
            that e.g. `aarch64` evaluation in the `platform_machine` works the
            same way irrespective if the marker uses `arm64` or `aarch64` value
            in the expression.

    Returns:
        {type}`bool` if the expression evaluation result or {type}`str` if the expression
        could not be evaluated.
    """
    var_name = None
    if right not in env and left not in env and not strict:
        return "{} {} {}".format(left, op, right)
    if left[0] == '"':
        var_name = right
        right = env[right]
        left = left.strip("\"")

        if _ENV_ALIASES in env:
            # On Windows, Linux, OSX different values may mean the same hardware,
            # e.g. Python on Windows returns arm64, but on Linux returns aarch64.
            # e.g. Python on Windows returns amd64, but on Linux returns x86_64.
            #
            # The following normalizes the values
            left = env.get(_ENV_ALIASES, {}).get(var_name, {}).get(left, left)

    else:
        var_name = left
        left = env[left]
        right = right.strip("\"")

        if _ENV_ALIASES in env:
            # See the note above on normalization
            right = env.get(_ENV_ALIASES, {}).get(var_name, {}).get(right, right)

    if var_name in _NON_VERSION_VAR_NAMES:
        return _env_expr(left, op, right)
    elif var_name.endswith("_version"):
        return _version_expr(left, op, right)
    else:
        # Do not fail here, just evaluate the expression to False.
        return False

def _env_expr(left, op, right):
    """Evaluate a string comparison expression"""
    if op == "==":
        return left == right
    elif op == "!=":
        return left != right
    elif op == "in":
        return left in right
    elif op == "not in":
        return left not in right
    elif op == "<":
        return left < right
    elif op == "<=":
        return left <= right
    elif op == ">":
        return left > right
    elif op == ">=":
        return left >= right
    else:
        return fail("unsupported op: '{}' {} '{}'".format(left, op, right))

def _version_expr(left, op, right):
    """Evaluate a version comparison expression"""
    _left = version.parse(left)
    _right = version.parse(right)
    if _left == None or _right == None:
        # Per spec, if either can't be normalized to a version, then
        # fallback to simple string comparison. Usually this is `platform_version`
        # or `platform_release`, which vary depending on platform.
        return _env_expr(left, op, right)

    if op == "===":
        return version.is_eeq(_left, _right)
    elif op == "!=":
        return version.is_ne(_left, _right)
    elif op == "==":
        return version.is_eq(_left, _right)
    elif op == "<":
        return version.is_lt(_left, _right)
    elif op == ">":
        return version.is_gt(_left, _right)
    elif op == "<=":
        return version.is_le(_left, _right)
    elif op == ">=":
        return version.is_ge(_left, _right)
    elif op == "~=":
        return version.is_compatible(_left, _right)
    else:
        return False  # Let's just ignore the invalid ops

# Code to allowing to combine expressions with logical operators

def _append(self, value):
    if value == None:
        return

    current = self.current() or self
    op = getattr(value, "op", None)

    if op == _NOT:
        current.tree.append(value)
    elif op in [_AND, _OR]:
        value.append(current.tree[-1])
        current.tree[-1] = value
    elif not current.tree:
        current.tree.append(value)
    elif hasattr(current.tree[-1], "append"):
        current.tree[-1].append(value)
    elif hasattr(current.tree, "_append"):
        current.tree._append(value)
    else:
        fail("Cannot evaluate '{}' in '{}', current: {}".format(value, self.marker, current))

def _open_parenthesis(self):
    """Add an extra node into the tree to perform evaluate inside parenthesis."""
    self._current.append(_new_expr(
        marker = self.marker,
        and_fn = self._and,
        or_fn = self._or,
        not_fn = self._not,
    ))

def _close_parenthesis(self):
    """Backtrack and evaluate the expression within parenthesis."""
    value = self._current.pop().value()
    if type(value) == type(""):
        return "({})".format(value)
    else:
        return value

def _not_expr(self):
    """Add an extra node into the tree to perform an 'not' operation."""

    def _append(value):
        """Append a value to the not expression node.

        This codifies `not` precedence over `and` and performs backtracking to
        evaluate any `not` statements and forward the value to the first `and`
        statement if needed.
        """

        current = self.current() or self
        current.tree[-1] = self._not(value)

        for _ in range(len(current.tree)):
            if not len(current.tree) > 1:
                break

            op = getattr(current.tree[-2], "op", None)
            if op == None:
                pass
            elif op == _NOT:
                value = current.tree.pop()
                current.tree[-1] = self._not(value)
                continue
            elif op == _AND:
                value = current.tree.pop()
                current.tree[-1].append(value)
            elif op != _OR:
                fail("BUG: '{} not' compound is unsupported".format(current.tree[-1]))

            break

    return struct(
        op = _NOT,
        append = _append,
    )

def _and_expr(self):
    """Add an extra node into the tree to perform an 'and' operation"""
    maybe_value = [None]

    def _append(value):
        """Append a value to the and expression node.

        Here we backtrack, but we only evaluate the current `and` statement -
        all of the `not` statements will be by now evaluated and `or`
        statements need to be evaluated later.
        """
        if maybe_value[0] == None:
            maybe_value[0] = value
            return

        current = self.current() or self
        current.tree[-1] = self._and(maybe_value[0], value)

    return struct(
        op = _AND,
        append = _append,
        # private fields that help debugging
        _maybe_value = maybe_value,
    )

def _or_expr(self):
    """Add an extra node into the tree to perform an 'or' operation"""
    maybe_value = [None]

    def _append(value):
        """Append a value to the or expression node.

        Here we just append the extra values to the tree and the `or`
        statements will be evaluated in the _value() function.
        """
        if maybe_value[0] == None:
            maybe_value[0] = value
            return

        current = self.current() or self
        current.tree.append(value)

    return struct(
        op = _OR,
        value = lambda x: self._or(maybe_value[0], x),
        append = _append,
        # private fields that help debugging
        _maybe_value = maybe_value,
    )
