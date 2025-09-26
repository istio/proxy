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

"""Tests for attr_builders."""

load("@rules_testing//lib:analysis_test.bzl", "analysis_test")
load("@rules_testing//lib:test_suite.bzl", "test_suite")
load("@rules_testing//lib:truth.bzl", "truth")
load("//python/private:attr_builders.bzl", "attrb")  # buildifier: disable=bzl-visibility

def _expect_cfg_defaults(expect, cfg):
    expect.where(expr = "cfg.outputs").that_collection(cfg.outputs()).contains_exactly([])
    expect.where(expr = "cfg.inputs").that_collection(cfg.inputs()).contains_exactly([])
    expect.where(expr = "cfg.implementation").that_bool(cfg.implementation()).equals(None)
    expect.where(expr = "cfg.target").that_bool(cfg.target()).equals(True)
    expect.where(expr = "cfg.exec_group").that_str(cfg.exec_group()).equals(None)
    expect.where(expr = "cfg.which_cfg").that_str(cfg.which_cfg()).equals("target")

_some_aspect = aspect(implementation = lambda target, ctx: None)
_SomeInfo = provider("MyInfo", fields = [])

_tests = []

def _report_failures(name, env):
    failures = env.failures

    def _report_failures_impl(env, target):
        _ = target  # @unused
        env._failures.extend(failures)

    analysis_test(
        name = name,
        target = "//python:none",
        impl = _report_failures_impl,
    )

# Calling attr.xxx() outside of the loading phase is an error, but rules_testing
# creates the expect/truth helpers during the analysis phase. To make the truth
# helpers available during the loading phase, fake out the ctx just enough to
# satify rules_testing.
def _loading_phase_expect(test_name):
    env = struct(
        ctx = struct(
            workspace_name = "bogus",
            label = Label(test_name),
            attr = struct(
                _impl_name = test_name,
            ),
        ),
        failures = [],
    )
    return env, truth.expect(env)

def _expect_builds(expect, builder, attribute_type):
    expect.that_str(str(builder.build())).contains(attribute_type)

def _test_cfg_arg(name):
    env, _ = _loading_phase_expect(name)

    def build_cfg(cfg):
        attrb.Label(cfg = cfg).build()

    build_cfg(None)
    build_cfg("target")
    build_cfg("exec")
    build_cfg(dict(exec_group = "eg"))
    build_cfg(dict(implementation = (lambda settings, attr: None)))
    build_cfg(config.exec())
    build_cfg(transition(
        implementation = (lambda settings, attr: None),
        inputs = [],
        outputs = [],
    ))

    # config.target is Bazel 8+
    if hasattr(config, "target"):
        build_cfg(config.target())

    # config.none is Bazel 8+
    if hasattr(config, "none"):
        build_cfg("none")
        build_cfg(config.none())

    _report_failures(name, env)

_tests.append(_test_cfg_arg)

def _test_bool(name):
    env, expect = _loading_phase_expect(name)
    subject = attrb.Bool()
    expect.that_str(subject.doc()).equals("")
    expect.that_bool(subject.default()).equals(False)
    expect.that_bool(subject.mandatory()).equals(False)
    _expect_builds(expect, subject, "attr.bool")

    subject.set_default(True)
    subject.set_mandatory(True)
    subject.set_doc("doc")

    expect.that_str(subject.doc()).equals("doc")
    expect.that_bool(subject.default()).equals(True)
    expect.that_bool(subject.mandatory()).equals(True)
    _expect_builds(expect, subject, "attr.bool")

    _report_failures(name, env)

_tests.append(_test_bool)

def _test_int(name):
    env, expect = _loading_phase_expect(name)

    subject = attrb.Int()
    expect.that_int(subject.default()).equals(0)
    expect.that_str(subject.doc()).equals("")
    expect.that_bool(subject.mandatory()).equals(False)
    expect.that_collection(subject.values()).contains_exactly([])
    _expect_builds(expect, subject, "attr.int")

    subject.set_default(42)
    subject.set_doc("doc")
    subject.set_mandatory(True)
    subject.values().append(42)

    expect.that_int(subject.default()).equals(42)
    expect.that_str(subject.doc()).equals("doc")
    expect.that_bool(subject.mandatory()).equals(True)
    expect.that_collection(subject.values()).contains_exactly([42])
    _expect_builds(expect, subject, "attr.int")

    _report_failures(name, env)

_tests.append(_test_int)

def _test_int_list(name):
    env, expect = _loading_phase_expect(name)

    subject = attrb.IntList()
    expect.that_bool(subject.allow_empty()).equals(True)
    expect.that_collection(subject.default()).contains_exactly([])
    expect.that_str(subject.doc()).equals("")
    expect.that_bool(subject.mandatory()).equals(False)
    _expect_builds(expect, subject, "attr.int_list")

    subject.default().append(99)
    subject.set_doc("doc")
    subject.set_mandatory(True)

    expect.that_collection(subject.default()).contains_exactly([99])
    expect.that_str(subject.doc()).equals("doc")
    expect.that_bool(subject.mandatory()).equals(True)
    _expect_builds(expect, subject, "attr.int_list")

    _report_failures(name, env)

_tests.append(_test_int_list)

def _test_label(name):
    env, expect = _loading_phase_expect(name)

    subject = attrb.Label()

    expect.that_str(subject.default()).equals(None)
    expect.that_str(subject.doc()).equals("")
    expect.that_bool(subject.mandatory()).equals(False)
    expect.that_bool(subject.executable()).equals(False)
    expect.that_bool(subject.allow_files()).equals(None)
    expect.that_bool(subject.allow_single_file()).equals(None)
    expect.that_collection(subject.providers()).contains_exactly([])
    expect.that_collection(subject.aspects()).contains_exactly([])
    _expect_cfg_defaults(expect, subject.cfg)
    _expect_builds(expect, subject, "attr.label")

    subject.set_default("//foo:bar")
    subject.set_doc("doc")
    subject.set_mandatory(True)
    subject.set_executable(True)
    subject.add_allow_files(".txt")
    subject.cfg.set_target()
    subject.providers().append(_SomeInfo)
    subject.aspects().append(_some_aspect)
    subject.cfg.outputs().append(Label("//some:output"))
    subject.cfg.inputs().append(Label("//some:input"))
    impl = lambda: None
    subject.cfg.set_implementation(impl)

    expect.that_str(subject.default()).equals("//foo:bar")
    expect.that_str(subject.doc()).equals("doc")
    expect.that_bool(subject.mandatory()).equals(True)
    expect.that_bool(subject.executable()).equals(True)
    expect.that_collection(subject.allow_files()).contains_exactly([".txt"])
    expect.that_bool(subject.allow_single_file()).equals(None)
    expect.that_collection(subject.providers()).contains_exactly([_SomeInfo])
    expect.that_collection(subject.aspects()).contains_exactly([_some_aspect])
    expect.that_collection(subject.cfg.outputs()).contains_exactly([Label("//some:output")])
    expect.that_collection(subject.cfg.inputs()).contains_exactly([Label("//some:input")])
    expect.that_bool(subject.cfg.implementation()).equals(impl)
    _expect_builds(expect, subject, "attr.label")

    _report_failures(name, env)

_tests.append(_test_label)

def _test_label_keyed_string_dict(name):
    env, expect = _loading_phase_expect(name)

    subject = attrb.LabelKeyedStringDict()

    expect.that_dict(subject.default()).contains_exactly({})
    expect.that_str(subject.doc()).equals("")
    expect.that_bool(subject.mandatory()).equals(False)
    expect.that_bool(subject.allow_files()).equals(False)
    expect.that_collection(subject.providers()).contains_exactly([])
    expect.that_collection(subject.aspects()).contains_exactly([])
    _expect_cfg_defaults(expect, subject.cfg)
    _expect_builds(expect, subject, "attr.label_keyed_string_dict")

    subject.default()["key"] = "//some:label"
    subject.set_doc("doc")
    subject.set_mandatory(True)
    subject.set_allow_files(True)
    subject.cfg.set_target()
    subject.providers().append(_SomeInfo)
    subject.aspects().append(_some_aspect)
    subject.cfg.outputs().append("//some:output")
    subject.cfg.inputs().append("//some:input")
    impl = lambda: None
    subject.cfg.set_implementation(impl)

    expect.that_dict(subject.default()).contains_exactly({"key": "//some:label"})
    expect.that_str(subject.doc()).equals("doc")
    expect.that_bool(subject.mandatory()).equals(True)
    expect.that_bool(subject.allow_files()).equals(True)
    expect.that_collection(subject.providers()).contains_exactly([_SomeInfo])
    expect.that_collection(subject.aspects()).contains_exactly([_some_aspect])
    expect.that_collection(subject.cfg.outputs()).contains_exactly(["//some:output"])
    expect.that_collection(subject.cfg.inputs()).contains_exactly(["//some:input"])
    expect.that_bool(subject.cfg.implementation()).equals(impl)

    _expect_builds(expect, subject, "attr.label_keyed_string_dict")

    subject.add_allow_files(".txt")
    expect.that_collection(subject.allow_files()).contains_exactly([".txt"])
    _expect_builds(expect, subject, "attr.label_keyed_string_dict")

    _report_failures(name, env)

_tests.append(_test_label_keyed_string_dict)

def _test_label_list(name):
    env, expect = _loading_phase_expect(name)

    subject = attrb.LabelList()

    expect.that_collection(subject.default()).contains_exactly([])
    expect.that_str(subject.doc()).equals("")
    expect.that_bool(subject.mandatory()).equals(False)
    expect.that_bool(subject.allow_files()).equals(False)
    expect.that_collection(subject.providers()).contains_exactly([])
    expect.that_collection(subject.aspects()).contains_exactly([])
    _expect_cfg_defaults(expect, subject.cfg)
    _expect_builds(expect, subject, "attr.label_list")

    subject.default().append("//some:label")
    subject.set_doc("doc")
    subject.set_mandatory(True)
    subject.set_allow_files([".txt"])
    subject.providers().append(_SomeInfo)
    subject.aspects().append(_some_aspect)

    expect.that_collection(subject.default()).contains_exactly(["//some:label"])
    expect.that_str(subject.doc()).equals("doc")
    expect.that_bool(subject.mandatory()).equals(True)
    expect.that_collection(subject.allow_files()).contains_exactly([".txt"])
    expect.that_collection(subject.providers()).contains_exactly([_SomeInfo])
    expect.that_collection(subject.aspects()).contains_exactly([_some_aspect])

    _expect_builds(expect, subject, "attr.label_list")

    _report_failures(name, env)

_tests.append(_test_label_list)

def _test_output(name):
    env, expect = _loading_phase_expect(name)

    subject = attrb.Output()
    expect.that_str(subject.doc()).equals("")
    expect.that_bool(subject.mandatory()).equals(False)
    _expect_builds(expect, subject, "attr.output")

    subject.set_doc("doc")
    subject.set_mandatory(True)
    expect.that_str(subject.doc()).equals("doc")
    expect.that_bool(subject.mandatory()).equals(True)
    _expect_builds(expect, subject, "attr.output")

    _report_failures(name, env)

_tests.append(_test_output)

def _test_output_list(name):
    env, expect = _loading_phase_expect(name)

    subject = attrb.OutputList()
    expect.that_bool(subject.allow_empty()).equals(True)
    expect.that_str(subject.doc()).equals("")
    expect.that_bool(subject.mandatory()).equals(False)
    _expect_builds(expect, subject, "attr.output_list")

    subject.set_allow_empty(False)
    subject.set_doc("doc")
    subject.set_mandatory(True)
    expect.that_bool(subject.allow_empty()).equals(False)
    expect.that_str(subject.doc()).equals("doc")
    expect.that_bool(subject.mandatory()).equals(True)
    _expect_builds(expect, subject, "attr.output_list")

    _report_failures(name, env)

_tests.append(_test_output_list)

def _test_string(name):
    env, expect = _loading_phase_expect(name)

    subject = attrb.String()
    expect.that_str(subject.default()).equals("")
    expect.that_str(subject.doc()).equals("")
    expect.that_bool(subject.mandatory()).equals(False)
    expect.that_collection(subject.values()).contains_exactly([])
    _expect_builds(expect, subject, "attr.string")

    subject.set_doc("doc")
    subject.set_mandatory(True)
    subject.values().append("green")
    expect.that_str(subject.doc()).equals("doc")
    expect.that_bool(subject.mandatory()).equals(True)
    expect.that_collection(subject.values()).contains_exactly(["green"])
    _expect_builds(expect, subject, "attr.string")

    _report_failures(name, env)

_tests.append(_test_string)

def _test_string_dict(name):
    env, expect = _loading_phase_expect(name)

    subject = attrb.StringDict()

    expect.that_dict(subject.default()).contains_exactly({})
    expect.that_str(subject.doc()).equals("")
    expect.that_bool(subject.mandatory()).equals(False)
    expect.that_bool(subject.allow_empty()).equals(True)
    _expect_builds(expect, subject, "attr.string_dict")

    subject.default()["key"] = "value"
    subject.set_doc("doc")
    subject.set_mandatory(True)
    subject.set_allow_empty(False)

    expect.that_dict(subject.default()).contains_exactly({"key": "value"})
    expect.that_str(subject.doc()).equals("doc")
    expect.that_bool(subject.mandatory()).equals(True)
    expect.that_bool(subject.allow_empty()).equals(False)
    _expect_builds(expect, subject, "attr.string_dict")

    _report_failures(name, env)

_tests.append(_test_string_dict)

def _test_string_keyed_label_dict(name):
    env, expect = _loading_phase_expect(name)

    subject = attrb.StringKeyedLabelDict()

    expect.that_dict(subject.default()).contains_exactly({})
    expect.that_str(subject.doc()).equals("")
    expect.that_bool(subject.mandatory()).equals(False)
    expect.that_bool(subject.allow_files()).equals(False)
    expect.that_collection(subject.providers()).contains_exactly([])
    expect.that_collection(subject.aspects()).contains_exactly([])
    _expect_cfg_defaults(expect, subject.cfg)
    _expect_builds(expect, subject, "attr.string_keyed_label_dict")

    subject.default()["key"] = "//some:label"
    subject.set_doc("doc")
    subject.set_mandatory(True)
    subject.set_allow_files([".txt"])
    subject.providers().append(_SomeInfo)
    subject.aspects().append(_some_aspect)

    expect.that_dict(subject.default()).contains_exactly({"key": "//some:label"})
    expect.that_str(subject.doc()).equals("doc")
    expect.that_bool(subject.mandatory()).equals(True)
    expect.that_collection(subject.allow_files()).contains_exactly([".txt"])
    expect.that_collection(subject.providers()).contains_exactly([_SomeInfo])
    expect.that_collection(subject.aspects()).contains_exactly([_some_aspect])

    _expect_builds(expect, subject, "attr.string_keyed_label_dict")

    _report_failures(name, env)

_tests.append(_test_string_keyed_label_dict)

def _test_string_list(name):
    env, expect = _loading_phase_expect(name)

    subject = attrb.StringList()

    expect.that_collection(subject.default()).contains_exactly([])
    expect.that_str(subject.doc()).equals("")
    expect.that_bool(subject.mandatory()).equals(False)
    expect.that_bool(subject.allow_empty()).equals(True)
    _expect_builds(expect, subject, "attr.string_list")

    subject.set_doc("doc")
    subject.set_mandatory(True)
    subject.default().append("blue")
    subject.set_allow_empty(False)
    expect.that_str(subject.doc()).equals("doc")
    expect.that_bool(subject.mandatory()).equals(True)
    expect.that_bool(subject.allow_empty()).equals(False)
    expect.that_collection(subject.default()).contains_exactly(["blue"])
    _expect_builds(expect, subject, "attr.string_list")

    _report_failures(name, env)

_tests.append(_test_string_list)

def _test_string_list_dict(name):
    env, expect = _loading_phase_expect(name)

    subject = attrb.StringListDict()

    expect.that_dict(subject.default()).contains_exactly({})
    expect.that_str(subject.doc()).equals("")
    expect.that_bool(subject.mandatory()).equals(False)
    expect.that_bool(subject.allow_empty()).equals(True)
    _expect_builds(expect, subject, "attr.string_list_dict")

    subject.set_doc("doc")
    subject.set_mandatory(True)
    subject.default()["key"] = ["red"]
    subject.set_allow_empty(False)
    expect.that_str(subject.doc()).equals("doc")
    expect.that_bool(subject.mandatory()).equals(True)
    expect.that_bool(subject.allow_empty()).equals(False)
    expect.that_dict(subject.default()).contains_exactly({"key": ["red"]})
    _expect_builds(expect, subject, "attr.string_list_dict")

    _report_failures(name, env)

_tests.append(_test_string_list_dict)

def attr_builders_test_suite(name):
    test_suite(
        name = name,
        tests = _tests,
    )
