"""Tests for WASI platform constraint mappings"""

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//rust/platform:triple_mappings.bzl", "triple_to_constraint_set")

def _wasi_platform_constraints_test_impl(ctx):
    env = unittest.begin(ctx)

    # Test WASI Preview 1 targets
    wasm32_wasip1_constraints = triple_to_constraint_set("wasm32-wasip1")
    asserts.equals(
        env,
        [
            "@platforms//cpu:wasm32",
            "@platforms//os:wasi",
            "@rules_rust//rust/platform:wasi_preview_1",
        ],
        wasm32_wasip1_constraints,
        "wasm32-wasip1 should map to WASI preview 1 constraints",
    )

    # Test backward compatibility
    wasm32_wasi_constraints = triple_to_constraint_set("wasm32-wasi")
    asserts.equals(
        env,
        [
            "@platforms//cpu:wasm32",
            "@platforms//os:wasi",
            "@rules_rust//rust/platform:wasi_preview_1",
        ],
        wasm32_wasi_constraints,
        "wasm32-wasi should map to WASI preview 1 for backward compatibility",
    )

    # Test WASI Preview 1 with threads
    wasm32_wasip1_threads_constraints = triple_to_constraint_set("wasm32-wasip1-threads")
    asserts.equals(
        env,
        [
            "@platforms//cpu:wasm32",
            "@platforms//os:wasi",
            "@rules_rust//rust/platform:wasi_preview_1",
        ],
        wasm32_wasip1_threads_constraints,
        "wasm32-wasip1-threads should map to WASI preview 1 constraints",
    )

    # Test WASI Preview 2
    wasm32_wasip2_constraints = triple_to_constraint_set("wasm32-wasip2")
    asserts.equals(
        env,
        [
            "@platforms//cpu:wasm32",
            "@platforms//os:wasi",
            "@rules_rust//rust/platform:wasi_preview_2",
        ],
        wasm32_wasip2_constraints,
        "wasm32-wasip2 should map to WASI preview 2 constraints",
    )

    # Test non-WASI wasm targets
    wasm32_unknown_constraints = triple_to_constraint_set("wasm32-unknown-unknown")
    asserts.equals(
        env,
        [
            "@platforms//cpu:wasm32",
            "@platforms//os:none",
        ],
        wasm32_unknown_constraints,
        "wasm32-unknown-unknown should not have WASI constraints",
    )

    wasm32_emscripten_constraints = triple_to_constraint_set("wasm32-unknown-emscripten")
    asserts.equals(
        env,
        [
            "@platforms//cpu:wasm32",
            "@platforms//os:emscripten",
        ],
        wasm32_emscripten_constraints,
        "wasm32-unknown-emscripten should map to emscripten OS",
    )

    # Test wasm64
    wasm64_unknown_constraints = triple_to_constraint_set("wasm64-unknown-unknown")
    asserts.equals(
        env,
        [
            "@platforms//cpu:wasm64",
            "@platforms//os:none",
        ],
        wasm64_unknown_constraints,
        "wasm64-unknown-unknown should map to wasm64 CPU",
    )

    return unittest.end(env)

wasi_platform_constraints_test = unittest.make(_wasi_platform_constraints_test_impl)

def wasi_platform_test_suite(name, **kwargs):
    """Define a test suite for WASI platform constraint mappings

    Args:
        name (str): The name of the test suite.
        **kwargs (dict): Additional keyword arguments for the test_suite.
    """
    wasi_platform_constraints_test(
        name = "wasi_platform_constraints_test",
    )

    native.test_suite(
        name = name,
        tests = [
            ":wasi_platform_constraints_test",
        ],
        **kwargs
    )
