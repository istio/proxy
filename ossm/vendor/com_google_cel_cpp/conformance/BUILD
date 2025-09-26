# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//conformance:run.bzl", "gen_conformance_tests")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

cc_library(
    name = "service",
    testonly = True,
    srcs = ["service.cc"],
    hdrs = ["service.h"],
    deps = [
        "//checker:optional",
        "//checker:standard_library",
        "//checker:type_checker_builder",
        "//checker:type_checker_builder_factory",
        "//common:ast",
        "//common:ast_proto",
        "//common:decl",
        "//common:decl_proto_v1alpha1",
        "//common:expr",
        "//common:source",
        "//common:type",
        "//common:value",
        "//common/internal:value_conversion",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public:transform_utility",
        "//extensions:bindings_ext",
        "//extensions:comprehensions_v2_functions",
        "//extensions:comprehensions_v2_macros",
        "//extensions:encoders",
        "//extensions:math_ext",
        "//extensions:math_ext_decls",
        "//extensions:math_ext_macros",
        "//extensions:proto_ext",
        "//extensions:strings",
        "//extensions/protobuf:enum_adapter",
        "//internal:status_macros",
        "//parser",
        "//parser:macro",
        "//parser:macro_expr_factory",
        "//parser:macro_registry",
        "//parser:options",
        "//parser:standard_macros",
        "//runtime",
        "//runtime:activation",
        "//runtime:constant_folding",
        "//runtime:optional_types",
        "//runtime:reference_resolver",
        "//runtime:runtime_options",
        "//runtime:standard_runtime_builder_factory",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_cel_spec//proto/cel/expr:syntax_cc_proto",
        "@com_google_cel_spec//proto/cel/expr/conformance/proto2:test_all_types_cc_proto",
        "@com_google_cel_spec//proto/cel/expr/conformance/proto3:test_all_types_cc_proto",
        "@com_google_googleapis//google/api/expr/conformance/v1alpha1:conformance_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_protobuf//:duration_cc_proto",
        "@com_google_protobuf//:empty_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//:struct_cc_proto",
        "@com_google_protobuf//:timestamp_cc_proto",
    ],
)

cc_library(
    name = "run",
    testonly = True,
    srcs = ["run.cc"],
    deps = [
        ":service",
        ":utils",
        "//internal:testing_no_main",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:span",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:value_cc_proto",
        "@com_google_cel_spec//proto/cel/expr/conformance/test:simple_cc_proto",
        "@com_google_googleapis//google/api/expr/conformance/v1alpha1:conformance_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_protobuf//src/google/protobuf/io",
    ],
    alwayslink = True,
)

cc_library(
    name = "utils",
    testonly = True,
    hdrs = ["utils.h"],
    deps = [
        "//internal:testing_no_main",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_cel_spec//proto/cel/expr:checked_cc_proto",
        "@com_google_cel_spec//proto/cel/expr:value_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_protobuf//:differencer",
        "@com_google_protobuf//:protobuf",
    ],
)

_ALL_TESTS = [
    "@com_google_cel_spec//tests/simple:testdata/basic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/bindings_ext.textproto",
    "@com_google_cel_spec//tests/simple:testdata/comparisons.textproto",
    "@com_google_cel_spec//tests/simple:testdata/conversions.textproto",
    "@com_google_cel_spec//tests/simple:testdata/dynamic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/encoders_ext.textproto",
    "@com_google_cel_spec//tests/simple:testdata/enums.textproto",
    "@com_google_cel_spec//tests/simple:testdata/fields.textproto",
    "@com_google_cel_spec//tests/simple:testdata/fp_math.textproto",
    "@com_google_cel_spec//tests/simple:testdata/integer_math.textproto",
    "@com_google_cel_spec//tests/simple:testdata/lists.textproto",
    "@com_google_cel_spec//tests/simple:testdata/logic.textproto",
    "@com_google_cel_spec//tests/simple:testdata/macros.textproto",
    "@com_google_cel_spec//tests/simple:testdata/math_ext.textproto",
    "@com_google_cel_spec//tests/simple:testdata/namespace.textproto",
    "@com_google_cel_spec//tests/simple:testdata/optionals.textproto",
    "@com_google_cel_spec//tests/simple:testdata/parse.textproto",
    "@com_google_cel_spec//tests/simple:testdata/plumbing.textproto",
    "@com_google_cel_spec//tests/simple:testdata/proto2.textproto",
    "@com_google_cel_spec//tests/simple:testdata/proto2_ext.textproto",
    "@com_google_cel_spec//tests/simple:testdata/proto3.textproto",
    "@com_google_cel_spec//tests/simple:testdata/string.textproto",
    "@com_google_cel_spec//tests/simple:testdata/string_ext.textproto",
    "@com_google_cel_spec//tests/simple:testdata/timestamps.textproto",
    "@com_google_cel_spec//tests/simple:testdata/unknowns.textproto",
    "@com_google_cel_spec//tests/simple:testdata/wrappers.textproto",
    "@com_google_cel_spec//tests/simple:testdata/block_ext.textproto",
    "@com_google_cel_spec//tests/simple:testdata/type_deduction.textproto",
]

_TESTS_TO_SKIP_MODERN = [
    # Tests which require spec changes.
    # TODO(issues/93): Deprecate Duration.getMilliseconds.
    "timestamps/duration_converters/get_milliseconds",

    # Broken test cases which should be supported.
    # TODO(issues/112): Unbound functions result in empty eval response.
    "basic/functions/unbound",
    "basic/functions/unbound_is_runtime_error",

    # TODO(issues/97): Parse-only qualified variable lookup "x.y" with binding "x.y" or "y" within container "x" fails
    "fields/qualified_identifier_resolution/qualified_ident,map_field_select,ident_with_longest_prefix_check,qualified_identifier_resolution_unchecked",
    "namespace/qualified/self_eval_qualified_lookup",
    "namespace/namespace/self_eval_container_lookup,self_eval_container_lookup_unchecked",
    # TODO(issues/117): Integer overflow on enum assignments should error.
    "enums/legacy_proto2/select_big,select_neg",

    # Skip until fixed.
    "wrappers/field_mask/to_json",
    "wrappers/empty/to_json",
    "fields/qualified_identifier_resolution/map_value_repeat_key_heterogeneous",

    # Future features for CEL 1.0
    # TODO(issues/119): Strong typing support for enums, specified but not implemented.
    "enums/strong_proto2",
    "enums/strong_proto3",

    # Not yet implemented.
    "string_ext/char_at",
    "string_ext/index_of",
    "string_ext/last_index_of",
    "string_ext/ascii_casing/upperascii",
    "string_ext/ascii_casing/upperascii_unicode",
    "string_ext/ascii_casing/upperascii_unicode_with_space",
    "string_ext/replace",
    "string_ext/substring",
    "string_ext/trim",
    "string_ext/quote",
    "string_ext/value_errors",
    "string_ext/type_errors",
]

_TESTS_TO_SKIP_MODERN_DASHBOARD = [
    # Future features for CEL 1.0
    # TODO(issues/119): Strong typing support for enums, specified but not implemented.
    "enums/strong_proto2",
    "enums/strong_proto3",
]

_TESTS_TO_SKIP_LEGACY = [
    # Tests which require spec changes.
    # TODO(issues/93): Deprecate Duration.getMilliseconds.
    "timestamps/duration_converters/get_milliseconds",

    # Broken test cases which should be supported.
    # TODO(issues/112): Unbound functions result in empty eval response.
    "basic/functions/unbound",
    "basic/functions/unbound_is_runtime_error",

    # TODO(issues/97): Parse-only qualified variable lookup "x.y" with binding "x.y" or "y" within container "x" fails
    "fields/qualified_identifier_resolution/qualified_ident,map_field_select,ident_with_longest_prefix_check,qualified_identifier_resolution_unchecked",
    "namespace/qualified/self_eval_qualified_lookup",
    "namespace/namespace/self_eval_container_lookup,self_eval_container_lookup_unchecked",
    # TODO(issues/117): Integer overflow on enum assignments should error.
    "enums/legacy_proto2/select_big,select_neg",

    # Skip until fixed.
    "wrappers/field_mask/to_json",
    "wrappers/empty/to_json",
    "fields/qualified_identifier_resolution/map_value_repeat_key_heterogeneous",

    # Future features for CEL 1.0
    # TODO(issues/119): Strong typing support for enums, specified but not implemented.
    "enums/strong_proto2",
    "enums/strong_proto3",

    # Legacy value does not support optional_type.
    "optionals/optionals",

    # Not yet implemented.
    "string_ext/char_at",
    "string_ext/index_of",
    "string_ext/last_index_of",
    "string_ext/ascii_casing/upperascii",
    "string_ext/ascii_casing/upperascii_unicode",
    "string_ext/ascii_casing/upperascii_unicode_with_space",
    "string_ext/replace",
    "string_ext/substring",
    "string_ext/trim",
    "string_ext/quote",
    "string_ext/value_errors",
    "string_ext/type_errors",

    # TODO(uncreated-issue/81): Fix null assignment to a field
    "proto2/set_null/list_value",
    "proto2/set_null/single_struct",
    "proto3/set_null/list_value",
    "proto3/set_null/single_struct",

    # cel.@block
    "block_ext/basic/optional_list",
    "block_ext/basic/optional_map",
    "block_ext/basic/optional_map_chained",
    "block_ext/basic/optional_message",
]

_TESTS_TO_SKIP_LEGACY_DASHBOARD = [
    # Future features for CEL 1.0
    # TODO(issues/119): Strong typing support for enums, specified but not implemented.
    "enums/strong_proto2",
    "enums/strong_proto3",

    # Legacy value does not support optional_type.
    "optionals/optionals",
]

# Generates a bunch of `cc_test` whose names follow the pattern
# `conformance_(...)_{arena|refcount}_{optimized|unoptimized}_{recursive|iterative}`.
gen_conformance_tests(
    name = "conformance_parse_only",
    data = _ALL_TESTS,
    modern = True,
    skip_tests = _TESTS_TO_SKIP_MODERN + ["type_deductions"],
)

gen_conformance_tests(
    name = "conformance_legacy_parse_only",
    data = _ALL_TESTS,
    modern = False,
    skip_tests = _TESTS_TO_SKIP_LEGACY + ["type_deductions"],
)

gen_conformance_tests(
    name = "conformance_checked",
    checked = True,
    data = _ALL_TESTS,
    modern = True,
    skip_tests = _TESTS_TO_SKIP_MODERN + [
        # block is a post-check optimization that inserts internal variables. The C++ type checker
        # needs support for a proper optimizer for this to work.
        "block_ext",
    ],
)

# Generates a bunch of `cc_test` whose names follow the pattern
# `conformance_dashboard_..._{arena|refcount}_{optimized|unoptimized}_{recursive|iterative}`.
gen_conformance_tests(
    name = "conformance_dashboard_parse_only",
    dashboard = True,
    data = _ALL_TESTS,
    modern = True,
    skip_tests = _TESTS_TO_SKIP_MODERN_DASHBOARD + ["type_deductions"],
    tags = [
        "guitar",
        "notap",
    ],
)

gen_conformance_tests(
    name = "conformance_dashboard_checked",
    checked = True,
    dashboard = True,
    data = _ALL_TESTS,
    modern = True,
    skip_tests = _TESTS_TO_SKIP_MODERN_DASHBOARD,
    tags = [
        "guitar",
        "notap",
    ],
)

gen_conformance_tests(
    name = "conformance_dashboard_legacy_parse_only",
    dashboard = True,
    data = _ALL_TESTS,
    modern = False,
    skip_tests = _TESTS_TO_SKIP_LEGACY_DASHBOARD + ["type_deductions"],
    tags = [
        "guitar",
        "notap",
    ],
)
