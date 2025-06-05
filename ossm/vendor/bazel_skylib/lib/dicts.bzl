# Copyright 2017 The Bazel Authors. All rights reserved.
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

"""Skylib module containing functions that operate on dictionaries."""

def _add(*dictionaries, **kwargs):
    """Returns a new `dict` that has all the entries of the given dictionaries.

    If the same key is present in more than one of the input dictionaries, the
    last of them in the argument list overrides any earlier ones.

    This function is designed to take zero or one arguments as well as multiple
    dictionaries, so that it follows arithmetic identities and callers can avoid
    special cases for their inputs: the sum of zero dictionaries is the empty
    dictionary, and the sum of a single dictionary is a copy of itself.

    Args:
      *dictionaries: Zero or more dictionaries to be added.
      **kwargs: Additional dictionary passed as keyword args.

    Returns:
      A new `dict` that has all the entries of the given dictionaries.
    """
    result = {}
    for d in dictionaries:
        result.update(d)
    result.update(kwargs)
    return result

def _omit(dictionary, keys):
    """Returns a new `dict` that has all the entries of `dictionary` with keys not in `keys`.

    Args:
      dictionary: A `dict`.
      keys: A sequence.

    Returns:
      A new `dict` that has all the entries of `dictionary` with keys not in `keys`.
    """
    keys_set = {k: None for k in keys}
    return {k: dictionary[k] for k in dictionary if k not in keys_set}

def _pick(dictionary, keys):
    """Returns a new `dict` that has all the entries of `dictionary` with keys in `keys`.

    Args:
      dictionary: A `dict`.
      keys: A sequence.

    Returns:
      A new `dict` that has all the entries of `dictionary` with keys in `keys`.
    """
    return {k: dictionary[k] for k in keys if k in dictionary}

dicts = struct(
    add = _add,
    omit = _omit,
    pick = _pick,
)
