"""Unit tests for pnpm utils
See https://docs.bazel.build/versions/main/skylark/testing.html#for-testing-starlark-utilities
"""

load("@bazel_skylib//lib:unittest.bzl", "asserts", "unittest")
load("//npm/private:utils.bzl", "utils")

# buildifier: disable=function-docstring
def test_strip_peer_dep_or_patched_version(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "21.1.0",
        utils.strip_peer_dep_or_patched_version("21.1.0_rollup@2.70.2_x@1.1.1"),
    )
    asserts.equals(env, "1.0.0", utils.strip_peer_dep_or_patched_version("1.0.0_o3deharooos255qt5xdujc3cuq"))
    asserts.equals(env, "21.1.0", utils.strip_peer_dep_or_patched_version("21.1.0"))
    return unittest.end(env)

def test_bazel_name(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "at_scope_pkg_21.1.0_rollup_2.70.2_at_scope_y_1.1.1",
        utils.bazel_name("@scope/pkg@21.1.0_rollup@2.70.2_@scope/y@1.1.1"),
    )
    asserts.equals(
        env,
        "at_scope_pkg_21.1.0",
        utils.bazel_name("@scope/pkg@21.1.0"),
    )
    return unittest.end(env)

# buildifier: disable=function-docstring
def test_pnpm_name(ctx):
    env = unittest.begin(ctx)
    asserts.equals(env, "@scope/y/1.1.1", utils.pnpm_name("@scope/y", "1.1.1"))
    asserts.equals(env, ("@scope/y", "1.1.1"), utils.parse_pnpm_package_key("@scope/y", "/@scope/y/1.1.1"))
    asserts.equals(env, ("@scope/y", "registry/@scope/y/1.1.1"), utils.parse_pnpm_package_key("@scope/y", "registry/@scope/y/1.1.1"))
    asserts.equals(env, ("@scope/y", "1.1.1"), utils.parse_pnpm_package_key("@scope/y", "1.1.1"))
    return unittest.end(env)

# buildifier: disable=function-docstring
def test_link_version(ctx):
    env = unittest.begin(ctx)
    asserts.equals(env, ("@scope/y", "0.0.0"), utils.parse_pnpm_package_key("@scope/y", "link:foo"))
    asserts.equals(env, ("@scope/y", "0.0.0"), utils.parse_pnpm_package_key("@scope/y", "file:bar"))
    asserts.equals(env, ("@scope/y", "0.0.0"), utils.parse_pnpm_package_key("@scope/y", "file:@foo/bar"))
    return unittest.end(env)

def test_friendly_name(ctx):
    env = unittest.begin(ctx)
    asserts.equals(env, "@scope/y@2.1.1", utils.friendly_name("@scope/y", "2.1.1"))
    return unittest.end(env)

def test_virtual_store_name(ctx):
    env = unittest.begin(ctx)
    asserts.equals(env, "@scope+y@2.1.1", utils.virtual_store_name("@scope/y", "2.1.1"))
    return unittest.end(env)

# buildifier: disable=function-docstring
def test_version_supported(ctx):
    env = unittest.begin(ctx)
    utils.assert_lockfile_version(5.3)
    utils.assert_lockfile_version(5.4)
    msg = utils.assert_lockfile_version(1.2, testonly = True)
    asserts.equals(env, "npm_translate_lock requires lock_version at least 5.3, but found 1.2. Please upgrade to pnpm v6 or greater.", msg)
    msg = utils.assert_lockfile_version(99.99, testonly = True)
    asserts.equals(env, "npm_translate_lock currently supports a maximum lock_version of 6.1, but found 99.99. Please file an issue on rules_js", msg)
    return unittest.end(env)

# buildifier: disable=function-docstring
def test_parse_package_name(ctx):
    env = unittest.begin(ctx)
    asserts.equals(env, ("@scope", "package"), utils.parse_package_name("@scope/package"))
    asserts.equals(env, ("@scope", "package/a"), utils.parse_package_name("@scope/package/a"))
    asserts.equals(env, ("", "package"), utils.parse_package_name("package"))
    asserts.equals(env, ("", "@package"), utils.parse_package_name("@package"))
    return unittest.end(env)

# buildifier: disable=function-docstring
def test_npm_registry_url(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "https://default",
        utils.npm_registry_url("a", {}, "https://default"),
    )
    asserts.equals(
        env,
        "http://default",
        utils.npm_registry_url("a", {}, "http://default"),
    )
    asserts.equals(
        env,
        "//default",
        utils.npm_registry_url("a", {}, "//default"),
    )
    asserts.equals(
        env,
        "https://default",
        utils.npm_registry_url("@a/b", {}, "https://default"),
    )
    asserts.equals(
        env,
        "https://default",
        utils.npm_registry_url("@a/b", {"@ab": "not me"}, "https://default"),
    )
    asserts.equals(
        env,
        "https://scoped-registry",
        utils.npm_registry_url("@a/b", {"@a": "https://scoped-registry"}, "https://default"),
    )
    return unittest.end(env)

# buildifier: disable=function-docstring
def test_npm_registry_download_url(ctx):
    env = unittest.begin(ctx)
    asserts.equals(
        env,
        "https://registry.npmjs.org/y/-/y-1.2.3.tgz",
        utils.npm_registry_download_url("y", "1.2.3", {}, "https://registry.npmjs.org/"),
    )
    asserts.equals(
        env,
        "http://registry.npmjs.org/y/-/y-1.2.3.tgz",
        utils.npm_registry_download_url("y", "1.2.3", {}, "http://registry.npmjs.org/"),
    )
    asserts.equals(
        env,
        "https://registry.npmjs.org/@scope/y/-/y-1.2.3.tgz",
        utils.npm_registry_download_url("@scope/y", "1.2.3", {}, "https://registry.npmjs.org/"),
    )
    asserts.equals(
        env,
        "https://registry.npmjs.org/@scope/y/-/y-1.2.3.tgz",
        utils.npm_registry_download_url("@scope/y", "1.2.3", {"@scopyy": "foobar"}, "https://registry.npmjs.org/"),
    )
    asserts.equals(
        env,
        "https://npm.pkg.github.com/@scope/y/-/y-1.2.3.tgz",
        utils.npm_registry_download_url("@scope/y", "1.2.3", {"@scope": "https://npm.pkg.github.com/"}, "https://registry.npmjs.org/"),
    )
    asserts.equals(
        env,
        "https://npm.pkg.github.com/@scope/y/-/y-1.2.3.tgz",
        utils.npm_registry_download_url("@scope/y", "1.2.3", {}, "https://npm.pkg.github.com/"),
    )
    return unittest.end(env)

t0_test = unittest.make(test_strip_peer_dep_or_patched_version)
t1_test = unittest.make(test_bazel_name)
t2_test = unittest.make(test_pnpm_name)
t3_test = unittest.make(test_friendly_name)
t4_test = unittest.make(test_virtual_store_name)
t5_test = unittest.make(test_version_supported)
t6_test = unittest.make(test_parse_package_name)
t7_test = unittest.make(test_npm_registry_download_url)
t8_test = unittest.make(test_npm_registry_url)
t9_test = unittest.make(test_link_version)

def utils_tests(name):
    unittest.suite(
        name,
        t0_test,
        t1_test,
        t2_test,
        t3_test,
        t4_test,
        t5_test,
        t6_test,
        t7_test,
        t8_test,
        t9_test,
    )
