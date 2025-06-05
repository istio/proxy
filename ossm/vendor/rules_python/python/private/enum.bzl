# Copyright 2024 The Bazel Authors. All rights reserved.
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

"""Enum-like object utilities

This is a separate file to minimize transitive loads.
"""

def enum(methods = {}, **kwargs):
    """Creates a struct whose primary purpose is to be like an enum.

    Args:
        methods: {type}`dict[str, callable]` functions that will be
            added to the created enum object, but will have the enum object
            itself passed as the first positional arg when calling them.
        **kwargs: The fields of the returned struct. All uppercase names will
            be treated as enum values and added to `__members__`.

    Returns:
        `struct` with the given values. It also has the field `__members__`,
        which is a dict of the enum names and values.
    """
    members = {
        key: value
        for key, value in kwargs.items()
        if key.upper() == key
    }

    for name, unbound_method in methods.items():
        # buildifier: disable=uninitialized
        kwargs[name] = lambda *a, **k: unbound_method(self, *a, **k)

    self = struct(__members__ = members, **kwargs)
    return self
