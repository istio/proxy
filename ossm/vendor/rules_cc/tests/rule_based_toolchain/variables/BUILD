load("//cc/toolchains:nested_args.bzl", "cc_nested_args")
load("//cc/toolchains/impl:variables.bzl", "cc_builtin_variables", "cc_variable", "types")
load("//tests/rule_based_toolchain:analysis_test_suite.bzl", "analysis_test_suite")
load(":variables_test.bzl", "TARGETS", "TESTS")

cc_variable(
    name = "str",
    type = types.string,
)

cc_variable(
    name = "optional_list",
    type = types.option(types.list(types.string)),
)

cc_variable(
    name = "str_list",
    type = types.list(types.string),
)

cc_variable(
    name = "str_option",
    type = types.option(types.string),
)

cc_variable(
    name = "struct",
    actions = ["//tests/rule_based_toolchain/actions:c_compile"],
    type = types.struct(
        nested_str = types.string,
        nested_str_list = types.list(types.string),
    ),
)

cc_variable(
    name = "struct_list",
    actions = ["//tests/rule_based_toolchain/actions:c_compile"],
    type = types.list(types.struct(
        nested_str = types.string,
        nested_str_list = types.list(types.string),
    )),
)

cc_variable(
    name = "struct_list.nested_str_list",
    type = types.unknown,
)

# Dots in the name confuse the test rules.
# It would end up generating targets.struct_list.nested_str_list.
alias(
    name = "nested_str_list",
    actual = ":struct_list.nested_str_list",
)

cc_nested_args(
    name = "simple_str",
    args = ["{str}"],
    format = {"str": ":str"},
)

cc_nested_args(
    name = "list_not_allowed",
    args = ["{s}"],
    format = {"s": ":str_list"},
)

cc_nested_args(
    name = "iterate_over_list",
    args = ["{}"],
    iterate_over = ":str_list",
)

cc_nested_args(
    name = "iterate_over_non_list",
    args = ["--foo"],
    iterate_over = ":str",
)

cc_nested_args(
    name = "str_not_a_bool",
    args = ["--foo"],
    requires_true = ":str",
)

cc_nested_args(
    name = "str_equal",
    args = ["--foo"],
    requires_equal = ":str",
    requires_equal_value = "bar",
)

cc_nested_args(
    name = "inner_iter",
    args = ["{}"],
    iterate_over = ":struct_list.nested_str_list",
)

cc_nested_args(
    name = "outer_iter",
    iterate_over = ":struct_list",
    nested = [":inner_iter"],
)

cc_nested_args(
    name = "bad_inner_iter",
    args = ["{s}"],
    format = {"s": ":struct_list.nested_str_list"},
)

cc_nested_args(
    name = "bad_outer_iter",
    iterate_over = ":struct_list",
    nested = [":bad_inner_iter"],
)

cc_nested_args(
    name = "bad_nested_optional",
    args = ["{s}"],
    format = {"s": ":str_option"},
)

cc_nested_args(
    name = "good_nested_optional",
    args = ["{s}"],
    format = {"s": ":str_option"},
    requires_not_none = ":str_option",
)

cc_nested_args(
    name = "optional_list_iter",
    args = ["--foo"],
    iterate_over = ":optional_list",
)

cc_builtin_variables(
    name = "variables",
    srcs = [
        ":optional_list",
        ":str",
        ":str_list",
        ":str_option",
        ":struct",
        ":struct_list",
    ],
)

cc_builtin_variables(
    name = "nested_variables",
    srcs = [
        ":struct_list.nested_str_list",
    ],
)

analysis_test_suite(
    name = "test_suite",
    targets = TARGETS,
    tests = TESTS,
)
