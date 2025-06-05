load("//cc/toolchains/impl:variables.bzl", "cc_variable", "types")
load("//tests/rule_based_toolchain:analysis_test_suite.bzl", "analysis_test_suite")
load(":nested_args_test.bzl", "TARGETS", "TESTS")

cc_variable(
    name = "foo",
    type = types.string,
)

cc_variable(
    name = "my_list",
    type = types.list(types.string),
)

analysis_test_suite(
    name = "test_suite",
    targets = TARGETS,
    tests = TESTS,
)
