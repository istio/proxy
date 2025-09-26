"Unit tests for pkg_glob"

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//npm/private:pkg_glob.bzl", "pkg_glob")

def _pkg_glob_test(ctx, pattern, matches, nonMatches):
    env = unittest.begin(ctx)

    for pkg in matches:
        asserts.equals(env, True, pkg_glob(pattern, pkg))

    for pkg in nonMatches:
        asserts.equals(env, False, pkg_glob(pattern, pkg))

    return unittest.end(env)

def _star(ctx):
    return _pkg_glob_test(ctx, "*", ["@eslint/plugin-foo", "express"], [])

def _ending_star(ctx):
    return _pkg_glob_test(ctx, "eslint-*", ["eslint-plugin-foo"], ["@eslint/plugin-foo", "express", "eslint", "-eslint"])

def _wrapping_star(ctx):
    return _pkg_glob_test(ctx, "*plugin*", ["eslint-plugin-foo", "@eslint/plugin-foo"], ["express"])

def _wrapped_star(ctx):
    return _pkg_glob_test(ctx, "a*c", ["ac", "abc", "accc", "acacac", "a1234c", "a12c34c"], ["abcd"])

def _starting_star(ctx):
    return _pkg_glob_test(ctx, "*-positive", ["is-positive"], ["is-positive-not"])

star_star_test = unittest.make(_star)
ending_star_test = unittest.make(_ending_star)
wrapping_star_test = unittest.make(_wrapping_star)
wrapped_star_test = unittest.make(_wrapped_star)
starting_star_test = unittest.make(_starting_star)

def pkg_glob_tests(name):
    unittest.suite(
        name,
        star_star_test,
        ending_star_test,
        wrapping_star_test,
        wrapped_star_test,
        starting_star_test,
    )
