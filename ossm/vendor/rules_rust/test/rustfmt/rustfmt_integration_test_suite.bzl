"""Test definitions for rustfmt test rules"""

load(
    "@rules_rust//rust:defs.bzl",
    "rust_binary",
    "rust_library",
    "rust_shared_library",
    "rust_static_library",
    "rustfmt_test",
)
load("@rules_shell//shell:sh_binary.bzl", "sh_binary")

_VARIANTS = {
    "rust_binary": rust_binary,
    "rust_library": rust_library,
    "rust_shared_library": rust_shared_library,
    "rust_static_library": rust_static_library,
}

def rustfmt_integration_test_suite(name, **kwargs):
    """Generate a test suite for rustfmt integration tests.

    Targets generated are expected to be executed using a target
    named: `{name}.test_runner`

    Args:
        name (str): The name of the test suite
        **kwargs (dict): Additional keyword arguments for the underlying test_suite.
    """

    tests = []
    for variant, rust_rule in _VARIANTS.items():
        #
        # Test edition 2018
        #
        rust_rule(
            name = "{}_formatted_2018".format(variant),
            srcs = ["srcs/2018/formatted.rs"],
            edition = "2018",
        )

        rustfmt_test(
            name = "{}_formatted_2018_test".format(variant),
            targets = [":{}_formatted_2018".format(variant)],
        )

        rust_rule(
            name = "{}_unformatted_2018".format(variant),
            srcs = ["srcs/2018/unformatted.rs"],
            edition = "2018",
            tags = ["norustfmt"],
        )

        rustfmt_test(
            name = "{}_unformatted_2018_test".format(variant),
            tags = ["manual"],
            targets = [":{}_unformatted_2018".format(variant)],
        )

        #
        # Test edition 2015
        #
        rust_rule(
            name = "{}_formatted_2015".format(variant),
            srcs = ["srcs/2015/formatted.rs"],
            edition = "2015",
        )

        rustfmt_test(
            name = "{}_formatted_2015_test".format(variant),
            targets = [":{}_formatted_2015".format(variant)],
        )

        rust_rule(
            name = "{}_unformatted_2015".format(variant),
            srcs = ["srcs/2015/unformatted.rs"],
            edition = "2015",
            tags = ["norustfmt"],
        )

        rustfmt_test(
            name = "{}_unformatted_2015_test".format(variant),
            tags = ["manual"],
            targets = [":{}_unformatted_2015".format(variant)],
        )

        #
        # Test targets with generated sources
        #
        rust_rule(
            name = "{}_generated".format(variant),
            srcs = [
                "srcs/generated/lib.rs",
                "srcs/generated/generated.rs",
            ],
            crate_root = "srcs/generated/lib.rs",
            edition = "2021",
        )

        rustfmt_test(
            name = "{}_generated_test".format(variant),
            targets = [":{}_generated".format(variant)],
        )

        tests.extend([
            "{}_formatted_2015_test".format(variant),
            "{}_formatted_2018_test".format(variant),
            "{}_unformatted_2015_test".format(variant),
            "{}_unformatted_2018_test".format(variant),
            "{}_generated_test".format(variant),
        ])

    native.test_suite(
        name = name,
        tests = tests,
        **kwargs
    )

    sh_binary(
        name = "{}.test_runner".format(name),
        srcs = ["rustfmt_failure_test.sh"],
        testonly = True,
    )
