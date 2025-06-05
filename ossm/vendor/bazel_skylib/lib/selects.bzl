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

"""Skylib module containing convenience interfaces for select()."""

def _with_or(input_dict, no_match_error = ""):
    """Drop-in replacement for `select()` that supports ORed keys.

    Example:

          ```build
          deps = selects.with_or({
              "//configs:one": [":dep1"],
              ("//configs:two", "//configs:three"): [":dep2or3"],
              "//configs:four": [":dep4"],
              "//conditions:default": [":default"]
          })
          ```

          Key labels may appear at most once anywhere in the input.

    Args:
      input_dict: The same dictionary `select()` takes, except keys may take
          either the usual form `"//foo:config1"` or
          `("//foo:config1", "//foo:config2", ...)` to signify
          `//foo:config1` OR `//foo:config2` OR `...`.
      no_match_error: Optional custom error to report if no condition matches.

    Returns:
      A native `select()` that expands

      `("//configs:two", "//configs:three"): [":dep2or3"]`

      to

      ```build
      "//configs:two": [":dep2or3"],
      "//configs:three": [":dep2or3"],
      ```
    """
    return select(_with_or_dict(input_dict), no_match_error = no_match_error)

def _with_or_dict(input_dict):
    """Variation of `with_or` that returns the dict of the `select()`.

    Unlike `select()`, the contents of the dict can be inspected by Starlark
    macros.

    Args:
      input_dict: Same as `with_or`.

    Returns:
      A dictionary usable by a native `select()`.
    """
    output_dict = {}
    for (key, value) in input_dict.items():
        if type(key) == type(()):
            for config_setting in key:
                if config_setting in output_dict.keys():
                    fail("key %s appears multiple times" % config_setting)
                output_dict[config_setting] = value
        else:
            if key in output_dict.keys():
                fail("key %s appears multiple times" % key)
            output_dict[key] = value
    return output_dict

def _config_setting_group(name, match_any = [], match_all = [], visibility = None):
    """Matches if all or any of its member `config_setting`s match.

    Example:

      ```build
      config_setting(name = "one", define_values = {"foo": "true"})
      config_setting(name = "two", define_values = {"bar": "false"})
      config_setting(name = "three", define_values = {"baz": "more_false"})

      config_setting_group(
          name = "one_two_three",
          match_all = [":one", ":two", ":three"]
      )

      cc_binary(
          name = "myapp",
          srcs = ["myapp.cc"],
          deps = select({
              ":one_two_three": [":special_deps"],
              "//conditions:default": [":default_deps"]
          })
      ```

    Args:
      name: The group's name. This is how `select()`s reference it.
      match_any: A list of `config_settings`. This group matches if *any* member
          in the list matches. If this is set, `match_all` must not be set.
      match_all: A list of `config_settings`. This group matches if *every*
          member in the list matches. If this is set, `match_any` must be not
          set.
      visibility: Visibility of the config_setting_group.
    """
    empty1 = not bool(len(match_any))
    empty2 = not bool(len(match_all))
    if (empty1 and empty2) or (not empty1 and not empty2):
        fail('Either "match_any" or "match_all" must be set, but not both.')
    _check_duplicates(match_any)
    _check_duplicates(match_all)

    if ((len(match_any) == 1 and match_any[0] == "//conditions:default") or
        (len(match_all) == 1 and match_all[0] == "//conditions:default")):
        # If the only entry is "//conditions:default", the condition is
        # automatically true.
        _config_setting_always_true(name, visibility)
    elif not empty1:
        _config_setting_or_group(name, match_any, visibility)
    else:
        _config_setting_and_group(name, match_all, visibility)

def _check_duplicates(settings):
    """Fails if any entry in settings appears more than once."""
    seen = {}
    for setting in settings:
        if setting in seen:
            fail(setting + " appears more than once. Duplicates not allowed.")
        seen[setting] = True

def _remove_default_condition(settings):
    """Returns settings with "//conditions:default" entries filtered out."""
    new_settings = []
    for setting in settings:
        if settings != "//conditions:default":
            new_settings.append(setting)
    return new_settings

def _config_setting_or_group(name, settings, visibility):
    """ORs multiple config_settings together (inclusively).

    The core idea is to create a sequential chain of alias targets where each is
    select-resolved as follows: If alias n matches config_setting n, the chain
    is true so it resolves to config_setting n. Else it resolves to alias n+1
    (which checks config_setting n+1, and so on). If none of the config_settings
    match, the final alias resolves to one of them arbitrarily, which by
    definition doesn't match.
    """

    # "//conditions:default" is present, the whole chain is automatically true.
    if len(_remove_default_condition(settings)) < len(settings):
        _config_setting_always_true(name, visibility)
        return

    elif len(settings) == 1:  # One entry? Just alias directly to it.
        native.alias(
            name = name,
            actual = settings[0],
            visibility = visibility,
        )
        return

    # We need n-1 aliases for n settings. The first alias has no extension. The
    # second alias is named name + "_2", and so on. For the first n-2 aliases,
    # if they don't match they reference the next alias over. If the n-1st alias
    # doesn't match, it references the final setting (which is then evaluated
    # directly to determine the final value of the AND chain).
    actual = [name + "_" + str(i) for i in range(2, len(settings))]
    actual.append(settings[-1])

    for i in range(1, len(settings)):
        native.alias(
            name = name if i == 1 else name + "_" + str(i),
            actual = select({
                settings[i - 1]: settings[i - 1],
                "//conditions:default": actual[i - 1],
            }),
            visibility = visibility if i == 1 else ["//visibility:private"],
        )

def _config_setting_and_group(name, settings, visibility):
    """ANDs multiple config_settings together.

    The core idea is to create a sequential chain of alias targets where each is
    select-resolved as follows: If alias n matches config_setting n, it resolves to
    alias n+1 (which evaluates config_setting n+1, and so on). Else it resolves to
    config_setting n, which doesn't match by definition. The only way to get a
    matching final result is if all config_settings match.
    """

    # "//conditions:default" is automatically true so doesn't need checking.
    settings = _remove_default_condition(settings)

    # One config_setting input? Just alias directly to it.
    if len(settings) == 1:
        native.alias(
            name = name,
            actual = settings[0],
            visibility = visibility,
        )
        return

    # We need n-1 aliases for n settings. The first alias has no extension. The
    # second alias is named name + "_2", and so on. For the first n-2 aliases,
    # if they match they reference the next alias over. If the n-1st alias matches,
    # it references the final setting (which is then evaluated directly to determine
    # the final value of the AND chain).
    actual = [name + "_" + str(i) for i in range(2, len(settings))]
    actual.append(settings[-1])

    for i in range(1, len(settings)):
        native.alias(
            name = name if i == 1 else name + "_" + str(i),
            actual = select({
                settings[i - 1]: actual[i - 1],
                "//conditions:default": settings[i - 1],
            }),
            visibility = visibility if i == 1 else ["//visibility:private"],
        )

def _config_setting_always_true(name, visibility):
    """Returns a config_setting with the given name that's always true.

    This is achieved by constructing a two-entry OR chain where each
    config_setting takes opposite values of a boolean flag.
    """
    name_on = name + "_stamp_binary_on_check"
    name_off = name + "_stamp_binary_off_check"
    native.config_setting(
        name = name_on,
        values = {"stamp": "1"},
    )
    native.config_setting(
        name = name_off,
        values = {"stamp": "0"},
    )
    return _config_setting_or_group(name, [":" + name_on, ":" + name_off], visibility)

selects = struct(
    with_or = _with_or,
    with_or_dict = _with_or_dict,
    config_setting_group = _config_setting_group,
)
