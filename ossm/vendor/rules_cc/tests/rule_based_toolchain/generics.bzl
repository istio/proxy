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
"""Implementation of a result type for use with rules_testing."""

load("@bazel_skylib//lib:structs.bzl", "structs")
load("@rules_testing//lib:truth.bzl", "subjects")

visibility("//tests/rule_based_toolchain/...")

def result_fn_wrapper(fn):
    """Wraps a function that may fail in a type similar to rust's Result type.

    An example usage is the following:
    # Implementation file
    def get_only(value, fail=fail):
        if len(value) == 1:
            return value[0]
        elif not value:
            fail("Unexpectedly empty")
        else:
            fail("%r had length %d, expected 1" % (value, len(value))

    # Test file
    load("...", _fn=fn)

    fn = result_fn_wrapper(_fn)
    int_result = result_subject(subjects.int)

    def my_test(env, _):
        env.expect.that_value(fn([]), factory=int_result)
            .err().equals("Unexpectedly empty")
        env.expect.that_value(fn([1]), factory=int_result)
            .ok().equals(1)
        env.expect.that_value(fn([1, 2]), factory=int_result)
            .err().contains("had length 2, expected 1")

    Args:
        fn: A function that takes in a parameter fail and calls it on failure.

    Returns:
        On success: struct(ok = <result>, err = None)
        On failure: struct(ok = None, err = <first error message>
    """

    def new_fn(*args, **kwargs):
        # Use a mutable type so that the fail_wrapper can modify this.
        failures = []

        def fail_wrapper(msg):
            failures.append(msg)

        result = fn(fail = fail_wrapper, *args, **kwargs)
        if failures:
            return struct(ok = None, err = failures[0])
        else:
            return struct(ok = result, err = None)

    return new_fn

def result_subject(factory):
    """A subject factory for Result<T>.

    Args:
        factory: A subject factory for T
    Returns:
        A subject factory for Result<T>
    """

    def new_factory(value, *, meta):
        def ok():
            if value.err != None:
                meta.add_failure("Wanted a value, but got an error", value.err)
            return factory(value.ok, meta = meta.derive("ok()"))

        def err():
            if value.err == None:
                meta.add_failure("Wanted an error, but got a value", value.ok)
            subject = subjects.str(value.err, meta = meta.derive("err()"))

            def contains_all_of(values):
                for value in values:
                    subject.contains(str(value))

            return struct(contains_all_of = contains_all_of, **structs.to_dict(subject))

        return struct(ok = ok, err = err)

    return new_factory

def optional_subject(factory):
    """A subject factory for Optional<T>.

    Args:
        factory: A subject factory for T
    Returns:
        A subject factory for Optional<T>
    """

    def new_factory(value, *, meta):
        def some():
            if value == None:
                meta.add_failure("Wanted a value, but got None", None)
            return factory(value, meta = meta)

        def is_none():
            if value != None:
                meta.add_failure("Wanted None, but got a value", value)

        return struct(some = some, is_none = is_none)

    return new_factory

# Curry subjects.struct so the type is actually generic.
struct_subject = lambda **attrs: lambda value, *, meta: subjects.struct(
    value,
    meta = meta,
    attrs = attrs,
)

# We can't do complex assertions on containers. This allows you to write
# assert.that_value({"foo": 1), factory=dict_key_subject(subjects.int))
#   .get("foo").equals(1)
dict_key_subject = lambda factory: lambda value, *, meta: struct(
    get = lambda key: factory(
        value[key],
        meta = meta.derive("get({})".format(key)),
    ),
    keys = lambda: subjects.collection(value.keys(), meta = meta.derive("keys()")),
    contains = lambda key: subjects.bool(key in value, meta = meta.derive("contains({})".format(key))),
)
