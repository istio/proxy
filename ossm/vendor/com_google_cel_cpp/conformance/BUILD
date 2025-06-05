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

load("//conformance:run.bzl", "gen_conformance_tests")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

cc_library(
    name = "value_conversion",
    srcs = ["value_conversion.cc"],
    hdrs = ["value_conversion.h"],
    deps = [
        "//common:any",
        "//common:type",
        "//common:value",
        "//common:value_kind",
        "//extensions/protobuf:value",
        "//internal:proto_time_encoding",
        "//internal:status_macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/types:optional",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:value_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "service",
    testonly = True,
    srcs = ["service.cc"],
    hdrs = ["service.h"],
    deps = [
        ":value_conversion",
        "//checker:optional",
        "//checker:standard_library",
        "//checker:type_checker_builder",
        "//common:ast",
        "//common:decl",
        "//common:expr",
        "//common:memory",
        "//common:source",
        "//common:type",
        "//common:value",
        "//eval/public:activation",
        "//eval/public:builtin_func_registrar",
        "//eval/public:cel_expr_builder_factory",
        "//eval/public:cel_expression",
        "//eval/public:cel_options",
        "//eval/public:cel_value",
        "//eval/public:transform_utility",
        "//extensions:bindings_ext",
        "//extensions:encoders",
        "//extensions:math_ext",
        "//extensions:math_ext_macros",
        "//extensions:proto_ext",
        "//extensions:strings",
        "//extensions/protobuf:ast_converters",
        "//extensions/protobuf:enum_adapter",
        "//extensions/protobuf:memory_manager",
        "//extensions/protobuf:value",
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
        "//runtime:managed_value_factory",
        "//runtime:optional_types",
        "//runtime:reference_resolver",
        "//runtime:runtime_options",
        "//runtime:standard_runtime_builder_factory",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_cel_spec//proto/test/v1/proto2:test_all_types_cc_proto",
        "@com_google_cel_spec//proto/test/v1/proto3:test_all_types_cc_proto",
        "@com_google_googleapis//google/api/expr/conformance/v1alpha1:conformance_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:syntax_cc_proto",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "run",
    testonly = True,
    srcs = ["run.cc"],
    deps = [
        ":service",
        "//internal:testing_no_main",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:span",
        "@com_google_cel_spec//proto/test/v1:simple_cc_proto",
        "@com_google_googleapis//google/api/expr/conformance/v1alpha1:conformance_cc_proto",
        "@com_google_googleapis//google/api/expr/v1alpha1:checked_cc_proto",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
    alwayslink = True,
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
]

_TESTS_TO_SKIP_MODERN = [
    # Tests which require spec changes.
    # TODO: Deprecate Duration.getMilliseconds.
    "timestamps/duration_converters/get_milliseconds",

    # Broken test cases which should be supported.
    # TODO: Unbound functions result in empty eval response.
    "basic/functions/unbound",
    "basic/functions/unbound_is_runtime_error",

    # TODO: Parse-only qualified variable lookup "x.y" wtih binding "x.y" or "y" within container "x" fails
    "fields/qualified_identifier_resolution/qualified_ident,map_field_select,ident_with_longest_prefix_check,qualified_identifier_resolution_unchecked",
    "namespace/qualified/self_eval_qualified_lookup",
    "namespace/namespace/self_eval_container_lookup,self_eval_container_lookup_unchecked",
    # TODO: Integer overflow on enum assignments should error.
    "enums/legacy_proto2/select_big,select_neg",

    # Skip until fixed.
    "wrappers/field_mask/to_json",
    "wrappers/empty/to_json",
    "fields/qualified_identifier_resolution/map_value_repeat_key_heterogeneous",

    # Future features for CEL 1.0
    # TODO: Strong typing support for enums, specified but not implemented.
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
    "string_ext/format",
    "string_ext/format_errors",
    "string_ext/value_errors",
    "string_ext/type_errors",

    # TODO: Fix null assignment to a field
    "proto2/set_null/single_message",
    "proto2/set_null/single_duration",
    "proto2/set_null/single_timestamp",
    "proto3/set_null/single_message",
    "proto3/set_null/single_duration",
    "proto3/set_null/single_timestamp",
    "wrappers/bool/to_null",
    "wrappers/int32/to_null",
    "wrappers/int64/to_null",
    "wrappers/uint32/to_null",
    "wrappers/uint64/to_null",
    "wrappers/float/to_null",
    "wrappers/double/to_null",
    "wrappers/bytes/to_null",
    "wrappers/string/to_null",

    # TODO: Add missing conversion function
    "conversions/bool",
]

_TESTS_TO_SKIP_MODERN_DASHBOARD = [
    # Future features for CEL 1.0
    # TODO: Strong typing support for enums, specified but not implemented.
    "enums/strong_proto2",
    "enums/strong_proto3",
]

_TESTS_TO_SKIP_LEGACY = [
    # Tests which require spec changes.
    # TODO: Deprecate Duration.getMilliseconds.
    "timestamps/duration_converters/get_milliseconds",

    # Broken test cases which should be supported.
    # TODO: Unbound functions result in empty eval response.
    "basic/functions/unbound",
    "basic/functions/unbound_is_runtime_error",

    # TODO: Parse-only qualified variable lookup "x.y" wtih binding "x.y" or "y" within container "x" fails
    "fields/qualified_identifier_resolution/qualified_ident,map_field_select,ident_with_longest_prefix_check,qualified_identifier_resolution_unchecked",
    "namespace/qualified/self_eval_qualified_lookup",
    "namespace/namespace/self_eval_container_lookup,self_eval_container_lookup_unchecked",
    # TODO: Integer overflow on enum assignments should error.
    "enums/legacy_proto2/select_big,select_neg",

    # Skip until fixed.
    "wrappers/field_mask/to_json",
    "wrappers/empty/to_json",
    "fields/qualified_identifier_resolution/map_value_repeat_key_heterogeneous",

    # Future features for CEL 1.0
    # TODO: Strong typing support for enums, specified but not implemented.
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
    "string_ext/format",
    "string_ext/format_errors",
    "string_ext/value_errors",
    "string_ext/type_errors",

    # TODO: Fix null assignment to a field
    "proto2/set_null/list_value",
    "proto2/set_null/single_struct",
    "proto3/set_null/list_value",
    "proto3/set_null/single_struct",

    # TODO: Add missing conversion function
    "conversions/bool",

    # cel.@block
    "block_ext/basic/optional_list",
    "block_ext/basic/optional_map",
    "block_ext/basic/optional_map_chained",
    "block_ext/basic/optional_message",
]

_TESTS_TO_SKIP_LEGACY_DASHBOARD = [
    # Future features for CEL 1.0
    # TODO: Strong typing support for enums, specified but not implemented.
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
    skip_tests = _TESTS_TO_SKIP_MODERN,
)

gen_conformance_tests(
    name = "conformance_legacy_parse_only",
    data = _ALL_TESTS,
    modern = False,
    skip_tests = _TESTS_TO_SKIP_LEGACY,
)

gen_conformance_tests(
    name = "conformance_checked",
    checked = True,
    data = _ALL_TESTS,
    modern = True,
    skip_tests = _TESTS_TO_SKIP_MODERN + [
        # TODO: Need to add function declarations for these extensions.
        "string_ext",
        "math_ext",
        "encoders_ext",
        # block is a post-check optimization that inserts internal variables. The C++ type checker
        # needs support for a proper optimizer for this to work.
        "block_ext",
        # Test has a typo, C++ conformance runner doesn't accept declaring a message type that isn't
        # known.
        "dynamic/any/var",
    ],
)

# Generates a bunch of `cc_test` whose names follow the pattern
# `conformance_dashboard_..._{arena|refcount}_{optimized|unoptimized}_{recursive|iterative}`.
gen_conformance_tests(
    name = "conformance_dashboard_parse_only",
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
    skip_tests = _TESTS_TO_SKIP_LEGACY_DASHBOARD,
    tags = [
        "guitar",
        "notap",
    ],
)
