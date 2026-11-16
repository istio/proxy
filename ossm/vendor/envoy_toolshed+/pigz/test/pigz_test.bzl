"""Tests for the prebuilt pigz hub rendering and the unavailable fallback."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts", "unittest")
load("//pigz:toolchain.bzl", "UNAVAILABLE_MESSAGE")
load("//pigz/private:pigz_prebuilt.bzl", "host_constraints", "host_platform", "render_pigz_toolchains_build")

_DEFS = "@" + "@envoy_toolshed+//pigz:defs.bzl"
_TOOLCHAIN_TYPE = "@" + "@envoy_toolshed+//pigz:toolchain_type"

def _render(platform_repos, host = None):
    return render_pigz_toolchains_build(platform_repos, _DEFS, _TOOLCHAIN_TYPE, host = host)

def _assert_platform(env, content, name, repo, cpu):
    asserts.true(env, "name = \"%s_impl\"" % name in content)
    asserts.true(env, "toolchain = \":%s_impl\"" % name in content)
    asserts.true(env, ("pigz = \"@" + "@%s//:pigz\"") % repo in content)
    asserts.true(env, "\"@platforms//cpu:%s\"" % cpu in content)

def _render_hub_build_test_impl(ctx):
    env = unittest.begin(ctx)

    content = _render({
        "linux-aarch64": "pigz_prebuilt_linux_aarch64",
        "linux-x86_64": "pigz_prebuilt_linux_x86_64",
    })
    asserts.true(env, ("load(\"%s\", \"pigz_toolchain\")" % _DEFS) in content)
    asserts.equals(env, 2, content.count("toolchain_type = \"%s\"" % _TOOLCHAIN_TYPE))
    asserts.equals(env, 2, content.count("\"@platforms//os:linux\""))

    # Matched on the platform the binary runs on, not the exec platform of the resolving target.
    asserts.equals(env, 2, content.count("target_compatible_with = ["))
    asserts.false(env, "exec_compatible_with" in content)
    _assert_platform(env, content, "linux_aarch64", "pigz_prebuilt_linux_aarch64", "aarch64")
    _assert_platform(env, content, "linux_x86_64", "pigz_prebuilt_linux_x86_64", "x86_64")

    content = _render({"linux-aarch64": None, "linux-x86_64": "pigz_prebuilt_linux_x86_64"})
    _assert_platform(env, content, "linux_x86_64", "pigz_prebuilt_linux_x86_64", "x86_64")
    asserts.false(env, "linux_aarch64" in content)
    asserts.false(env, "@platforms//cpu:aarch64" in content)

    content = _render({"linux-aarch64": "pigz_prebuilt_linux_aarch64", "linux-x86_64": None})
    _assert_platform(env, content, "linux_aarch64", "pigz_prebuilt_linux_aarch64", "aarch64")
    asserts.false(env, "linux_x86_64" in content)
    asserts.false(env, "@platforms//cpu:x86_64" in content)

    # Both disabled (the default until a bins release publishes hashes): an empty hub, so only
    # the source fallback can match.
    content = _render({"linux-aarch64": None, "linux-x86_64": None})
    asserts.false(env, "pigz_toolchain(" in content)
    asserts.false(env, "toolchain(" in content)

    # A host pigz (`setup(host_path = ...)`), for a host without a prebuilt: a toolchain over
    # `host/pigz` (through a filegroup, as a label to a file), alongside the prebuilts.
    host = ["@platforms//os:macos", "@platforms//cpu:aarch64"]
    content = _render({
        "linux-aarch64": "pigz_prebuilt_linux_aarch64",
        "linux-x86_64": "pigz_prebuilt_linux_x86_64",
    }, host = host)
    asserts.true(env, "srcs = [\"host/pigz\"]" in content)
    asserts.true(env, "pigz = \":host_pigz\"" in content)
    asserts.true(env, "toolchain = \":host_impl\"" in content)
    asserts.equals(env, 3, content.count("toolchain_type = \"%s\"" % _TOOLCHAIN_TYPE))
    asserts.true(env, "\"@platforms//os:macos\"" in content)

    # Only the host: no prebuilts (eg all disabled).
    content = _render({"linux-aarch64": None, "linux-x86_64": None}, host = host)
    asserts.equals(env, 1, content.count("\ntoolchain("))
    asserts.equals(env, 1, content.count("\npigz_toolchain("))
    asserts.true(env, "name = \"host\"" in content)
    return unittest.end(env)

def _host_constraints_test_impl(ctx):
    env = unittest.begin(ctx)

    # The host's platform, named as the prebuilts' (so an enabled prebuilt for it is found).
    asserts.equals(env, "linux-x86_64", host_platform("Linux", "amd64"))
    asserts.equals(env, "linux-aarch64", host_platform("linux", "aarch64"))
    asserts.equals(env, "macos-aarch64", host_platform("Mac OS X", "arm64"))
    asserts.equals(env, "macos-x86_64", host_platform("mac os x", "x86_64"))
    asserts.equals(env, None, host_platform("windows 10", "amd64"))
    asserts.equals(env, None, host_platform("linux", "s390x"))

    asserts.equals(
        env,
        ["@platforms//os:linux", "@platforms//cpu:x86_64"],
        host_constraints("linux-x86_64"),
    )
    asserts.equals(
        env,
        ["@platforms//os:macos", "@platforms//cpu:aarch64"],
        host_constraints("macos-aarch64"),
    )
    return unittest.end(env)

host_constraints_test = unittest.make(_host_constraints_test_impl)

render_hub_build_test = unittest.make(_render_hub_build_test_impl)

def _unavailable_test_impl(ctx):
    env = analysistest.begin(ctx)
    asserts.expect_failure(env, UNAVAILABLE_MESSAGE)
    return analysistest.end(env)

# Resolves `//pigz` for a platform with no pigz toolchain other than the unavailable fallback.
unavailable_test = analysistest.make(
    _unavailable_test_impl,
    expect_failure = True,
    config_settings = {
        "//command_line_option:platforms": str(Label("//pigz/test:no_pigz_platform")),
    },
)
