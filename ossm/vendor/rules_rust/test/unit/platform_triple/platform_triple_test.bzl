"""Tests for the platform triple constructor"""

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//rust/platform:triple.bzl", "triple")
load("//rust/platform:triple_mappings.bzl", "SUPPORTED_PLATFORM_TRIPLES")

def _construct_platform_triple_test_impl(ctx):
    env = unittest.begin(ctx)

    imaginary_triple = triple("arch-vendor-system-abi")

    asserts.equals(
        env,
        "arch",
        imaginary_triple.arch,
    )

    asserts.equals(
        env,
        "vendor",
        imaginary_triple.vendor,
    )

    asserts.equals(
        env,
        "system",
        imaginary_triple.system,
    )

    asserts.equals(
        env,
        "abi",
        imaginary_triple.abi,
    )

    asserts.equals(
        env,
        "arch-vendor-system-abi",
        imaginary_triple.str,
    )

    return unittest.end(env)

def _construct_minimal_platform_triple_test_impl(ctx):
    env = unittest.begin(ctx)

    imaginary_triple = triple("arch-vendor-system")

    asserts.equals(
        env,
        "arch",
        imaginary_triple.arch,
    )

    asserts.equals(
        env,
        "vendor",
        imaginary_triple.vendor,
    )

    asserts.equals(
        env,
        "system",
        imaginary_triple.system,
    )

    asserts.equals(
        env,
        None,
        imaginary_triple.abi,
    )

    asserts.equals(
        env,
        "arch-vendor-system",
        imaginary_triple.str,
    )

    return unittest.end(env)

def _supported_platform_triples_test_impl(ctx):
    env = unittest.begin(ctx)

    for supported_triple in SUPPORTED_PLATFORM_TRIPLES:
        asserts.equals(
            env,
            supported_triple,
            triple(supported_triple).str,
        )

    return unittest.end(env)

def _assert_parts(env, triple, arch, vendor, system, abi):
    asserts.equals(
        env,
        arch,
        triple.arch,
        "{} did not parse {} correctly".format(triple.str, "arch"),
    )
    asserts.equals(
        env,
        vendor,
        triple.vendor,
        "{} did not parse {} correctly".format(triple.str, "vendor"),
    )
    asserts.equals(
        env,
        system,
        triple.system,
        "{} did not parse {} correctly".format(triple.str, "system"),
    )
    asserts.equals(
        env,
        abi,
        triple.abi,
        "{} did not parse {} correctly".format(triple.str, "abi"),
    )

def _construct_known_triples_test_impl(ctx):
    env = unittest.begin(ctx)

    _assert_parts(env, triple("aarch64-apple-darwin"), "aarch64", "apple", "darwin", None)
    _assert_parts(env, triple("aarch64-fuchsia"), "aarch64", "unknown", "fuchsia", None)
    _assert_parts(env, triple("aarch64-unknown-linux-musl"), "aarch64", "unknown", "linux", "musl")
    _assert_parts(env, triple("thumbv7em-none-eabi"), "thumbv7em", None, "none", "eabi")
    _assert_parts(env, triple("thumbv8m.main-none-eabi"), "thumbv8m.main", None, "none", "eabi")
    _assert_parts(env, triple("wasm32-unknown-unknown"), "wasm32", "unknown", "unknown", None)
    _assert_parts(env, triple("wasm32-wasi"), "wasm32", "wasip1", "wasip1", None)
    _assert_parts(env, triple("wasm32-wasip1"), "wasm32", "wasip1", "wasip1", None)
    _assert_parts(env, triple("x86_64-fuchsia"), "x86_64", "unknown", "fuchsia", None)

    return unittest.end(env)

construct_platform_triple_test = unittest.make(_construct_platform_triple_test_impl)
construct_minimal_platform_triple_test = unittest.make(_construct_minimal_platform_triple_test_impl)
supported_platform_triples_test = unittest.make(_supported_platform_triples_test_impl)
construct_known_triples_test = unittest.make(_construct_known_triples_test_impl)

def platform_triple_test_suite(name, **kwargs):
    """Define a test suite for testing the `triple` constructor

    Args:
        name (str): The name of the test suite.
        **kwargs (dict): Additional keyword arguments for the test_suite.
    """
    construct_platform_triple_test(
        name = "construct_platform_triple_test",
    )
    construct_minimal_platform_triple_test(
        name = "construct_minimal_platform_triple_test",
    )
    supported_platform_triples_test(
        name = "supported_platform_triples_test",
    )
    construct_known_triples_test(
        name = "construct_known_triples_test",
    )

    native.test_suite(
        name = name,
        tests = [
            ":construct_platform_triple_test",
            ":construct_minimal_platform_triple_test",
            ":supported_platform_triples_test",
            ":construct_known_triples_test",
        ],
        **kwargs
    )
