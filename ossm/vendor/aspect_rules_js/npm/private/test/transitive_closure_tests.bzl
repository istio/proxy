"""Unit tests for pnpm utils
See https://docs.bazel.build/versions/main/skylark/testing.html#for-testing-starlark-utilities
"""

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//npm/private:transitive_closure.bzl", "gather_transitive_closure")

TEST_PACKAGES = {
    "@aspect-test/a/5.0.0": {
        "name": "@aspect-test/a",
        "version": "5.0.0",
        "integrity": "sha512-t/lwpVXG/jmxTotGEsmjwuihC2Lvz/Iqt63o78SI3O5XallxtFp5j2WM2M6HwkFiii9I42KdlAF8B3plZMz0Fw==",
        "dependencies": {
            "@aspect-test/b": "5.0.0",
            "@aspect-test/c": "1.0.0",
            "@aspect-test/d": "2.0.0_@aspect-test+c@1.0.0",
        },
        "optional_dependencies": {},
    },
    "@aspect-test/b/5.0.0": {
        "dependencies": {},
        "optional_dependencies": {
            "@aspect-test/c": "2.0.0",
        },
    },
    "@aspect-test/c/1.0.0": {
        "dependencies": {},
        "optional_dependencies": {},
    },
    "@aspect-test/c/2.0.0": {
        "dependencies": {},
        "optional_dependencies": {},
    },
    "@aspect-test/d/2.0.0_@aspect-test+c@1.0.0": {
        "dependencies": {},
        "optional_dependencies": {},
    },
}

# buildifier: disable=function-docstring
def test_walk_deps(ctx):
    env = unittest.begin(ctx)
    no_optional = True
    not_no_optional = False

    # Walk the example tree above
    closure = gather_transitive_closure(TEST_PACKAGES, "@aspect-test/a/5.0.0", not_no_optional)
    expected = {"@aspect-test/a": ["5.0.0"], "@aspect-test/b": ["5.0.0"], "@aspect-test/c": ["1.0.0", "2.0.0"], "@aspect-test/d": ["2.0.0_@aspect-test+c@1.0.0"]}
    asserts.equals(env, expected, closure)

    # Run again with no_optional set, this means we shouldn't walk the dep from @aspect-test/b/5.0.0 -> @aspect-test/c/2.0.0
    closure = gather_transitive_closure(TEST_PACKAGES, "@aspect-test/a/5.0.0", no_optional)
    expected = {"@aspect-test/a": ["5.0.0"], "@aspect-test/b": ["5.0.0"], "@aspect-test/c": ["1.0.0"], "@aspect-test/d": ["2.0.0_@aspect-test+c@1.0.0"]}
    asserts.equals(env, expected, closure)

    return unittest.end(env)

t0_test = unittest.make(test_walk_deps)

def transitive_closure_tests(name):
    unittest.suite(name, t0_test)
