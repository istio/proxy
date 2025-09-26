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

"""Tests for rule_builders."""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:util.bzl", "TestingAspectInfo")
load("//python/private:attr_builders.bzl", "attrb")  # buildifier: disable=bzl-visibility
load("//python/private:rule_builders.bzl", "ruleb")  # buildifier: disable=bzl-visibility

RuleInfo = provider(doc = "test provider", fields = [])

_tests = []  # analysis-phase tests
_basic_tests = []  # loading-phase tests

fruit = ruleb.Rule(
    implementation = lambda ctx: [RuleInfo()],
    attrs = {
        "color": attrb.String(default = "yellow"),
        "fertilizers": attrb.LabelList(
            allow_files = True,
        ),
        "flavors": attrb.StringList(),
        "nope": attr.label(
            # config.none is Bazel 8+
            cfg = config.none() if hasattr(config, "none") else None,
        ),
        "organic": lambda: attrb.Bool(),
        "origin": lambda: attrb.Label(),
        "size": lambda: attrb.Int(default = 10),
    },
).build()

def _test_fruit_rule(name):
    fruit(
        name = name + "_subject",
        flavors = ["spicy", "sweet"],
        organic = True,
        size = 5,
        origin = "//python:none",
        fertilizers = [
            "nitrogen.txt",
            "phosphorus.txt",
        ],
    )

    analysis_test(
        name = name,
        target = name + "_subject",
        impl = _test_fruit_rule_impl,
    )

def _test_fruit_rule_impl(env, target):
    attrs = target[TestingAspectInfo].attrs
    env.expect.that_str(attrs.color).equals("yellow")
    env.expect.that_collection(attrs.flavors).contains_exactly(["spicy", "sweet"])
    env.expect.that_bool(attrs.organic).equals(True)
    env.expect.that_int(attrs.size).equals(5)

    # //python:none is an alias to //python/private:sentinel; we see the
    # resolved value, not the intermediate alias
    env.expect.that_target(attrs.origin).label().equals(Label("//python/private:sentinel"))

    env.expect.that_collection(attrs.fertilizers).transform(
        desc = "target.label",
        map_each = lambda t: t.label,
    ).contains_exactly([
        Label(":nitrogen.txt"),
        Label(":phosphorus.txt"),
    ])

_tests.append(_test_fruit_rule)

# NOTE: `Rule.build()` can't be called because it's not during the top-level
# bzl evaluation.
def _test_rule_api(env):
    subject = ruleb.Rule()
    expect = env.expect

    expect.that_dict(subject.attrs.map).contains_exactly({})
    expect.that_collection(subject.cfg.outputs()).contains_exactly([])
    expect.that_collection(subject.cfg.inputs()).contains_exactly([])
    expect.that_bool(subject.cfg.implementation()).equals(None)
    expect.that_str(subject.doc()).equals("")
    expect.that_dict(subject.exec_groups()).contains_exactly({})
    expect.that_bool(subject.executable()).equals(False)
    expect.that_collection(subject.fragments()).contains_exactly([])
    expect.that_bool(subject.implementation()).equals(None)
    expect.that_collection(subject.provides()).contains_exactly([])
    expect.that_bool(subject.test()).equals(False)
    expect.that_collection(subject.toolchains()).contains_exactly([])

    subject.attrs.update({
        "builder": attrb.String(),
        "factory": lambda: attrb.String(),
    })
    subject.attrs.put("put_factory", lambda: attrb.Int())
    subject.attrs.put("put_builder", attrb.Int())

    expect.that_dict(subject.attrs.map).keys().contains_exactly([
        "factory",
        "builder",
        "put_factory",
        "put_builder",
    ])
    expect.that_collection(subject.attrs.map.values()).transform(
        desc = "type() of attr value",
        map_each = type,
    ).contains_exactly(["struct", "struct", "struct", "struct"])

    subject.set_doc("doc")
    expect.that_str(subject.doc()).equals("doc")

    subject.exec_groups()["eg"] = ruleb.ExecGroup()
    expect.that_dict(subject.exec_groups()).keys().contains_exactly(["eg"])

    subject.set_executable(True)
    expect.that_bool(subject.executable()).equals(True)

    subject.fragments().append("frag")
    expect.that_collection(subject.fragments()).contains_exactly(["frag"])

    impl = lambda: None
    subject.set_implementation(impl)
    expect.that_bool(subject.implementation()).equals(impl)

    subject.provides().append(RuleInfo)
    expect.that_collection(subject.provides()).contains_exactly([RuleInfo])

    subject.set_test(True)
    expect.that_bool(subject.test()).equals(True)

    subject.toolchains().append(ruleb.ToolchainType())
    expect.that_collection(subject.toolchains()).has_size(1)

    expect.that_collection(subject.cfg.outputs()).contains_exactly([])
    expect.that_collection(subject.cfg.inputs()).contains_exactly([])
    expect.that_bool(subject.cfg.implementation()).equals(None)

    subject.cfg.set_implementation(impl)
    expect.that_bool(subject.cfg.implementation()).equals(impl)
    subject.cfg.add_inputs(Label("//some:input"))
    expect.that_collection(subject.cfg.inputs()).contains_exactly([
        Label("//some:input"),
    ])
    subject.cfg.add_outputs(Label("//some:output"))
    expect.that_collection(subject.cfg.outputs()).contains_exactly([
        Label("//some:output"),
    ])

_basic_tests.append(_test_rule_api)

def _test_exec_group(env):
    subject = ruleb.ExecGroup()

    env.expect.that_collection(subject.toolchains()).contains_exactly([])
    env.expect.that_collection(subject.exec_compatible_with()).contains_exactly([])
    env.expect.that_str(str(subject.build())).contains("ExecGroup")

    subject.toolchains().append(ruleb.ToolchainType("//python:none"))
    subject.exec_compatible_with().append("//some:constraint")
    env.expect.that_str(str(subject.build())).contains("ExecGroup")

_basic_tests.append(_test_exec_group)

def _test_toolchain_type(env):
    subject = ruleb.ToolchainType()

    env.expect.that_str(subject.name()).equals(None)
    env.expect.that_bool(subject.mandatory()).equals(True)
    subject.set_name("//some:toolchain_type")
    env.expect.that_str(str(subject.build())).contains("ToolchainType")

    subject.set_name("//some:toolchain_type")
    subject.set_mandatory(False)
    env.expect.that_str(subject.name()).equals("//some:toolchain_type")
    env.expect.that_bool(subject.mandatory()).equals(False)
    env.expect.that_str(str(subject.build())).contains("ToolchainType")

_basic_tests.append(_test_toolchain_type)

rule_with_toolchains = ruleb.Rule(
    implementation = lambda ctx: [],
    toolchains = [
        ruleb.ToolchainType("//tests/builders:tct_1", mandatory = False),
        lambda: ruleb.ToolchainType("//tests/builders:tct_2", mandatory = False),
        "//tests/builders:tct_3",
        Label("//tests/builders:tct_4"),
    ],
    exec_groups = {
        "eg1": ruleb.ExecGroup(
            toolchains = [
                ruleb.ToolchainType("//tests/builders:tct_1", mandatory = False),
                lambda: ruleb.ToolchainType("//tests/builders:tct_2", mandatory = False),
                "//tests/builders:tct_3",
                Label("//tests/builders:tct_4"),
            ],
        ),
        "eg2": lambda: ruleb.ExecGroup(),
    },
).build()

def _test_rule_with_toolchains(name):
    rule_with_toolchains(
        name = name + "_subject",
        tags = ["manual"],  # Can't be built without extra_toolchains set
    )

    analysis_test(
        name = name,
        impl = lambda env, target: None,
        target = name + "_subject",
        config_settings = {
            "//command_line_option:extra_toolchains": [
                Label("//tests/builders:all"),
            ],
        },
    )

_tests.append(_test_rule_with_toolchains)

rule_with_immutable_attrs = ruleb.Rule(
    implementation = lambda ctx: [],
    attrs = {
        "foo": attr.string(),
    },
).build()

def _test_rule_with_immutable_attrs(name):
    rule_with_immutable_attrs(name = name + "_subject")
    analysis_test(
        name = name,
        target = name + "_subject",
        impl = lambda env, target: None,
    )

_tests.append(_test_rule_with_immutable_attrs)

def rule_builders_test_suite(name):
    test_suite(
        name = name,
        basic_tests = _basic_tests,
        tests = _tests,
    )
