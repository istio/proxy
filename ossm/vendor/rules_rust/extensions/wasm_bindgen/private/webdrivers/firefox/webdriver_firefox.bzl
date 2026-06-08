"""Depednencies for `wasm_bindgen_test` rules"""

load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//private/webdrivers:webdriver_utils.bzl", "build_file_repository", "webdriver_repository")

_FIREFOX_WRAPPER_TEMPLATE_UNIX = """\
#!/usr/bin/env bash

set -euo pipefail

exec {firefox} $@
"""

_FIREFOX_WRAPPER_TEMPLATE_WINDOWS = """\
@ECHO OFF

{firefox} %*

:: Capture the exit code of firefox.exe
SET exit_code=!errorlevel!

:: Exit with the same exit code
EXIT /b %exit_code%
"""

_FIREFOX_NOT_FOUND_TEMPLATE_UNIX = """\
#!/usr/bin/env bash

set -euo pipefail

>&2 echo "No firefox binary provided. Please export 'FIREFOX_BINARY' and try building again"
exit 1
"""

_FIREFOX_NOT_FOUND_TEMPLATE_WINDOWS = """\
@ECHO OFF

echo No firefox binary provided. Please export 'FIREFOX_BINARY' and try building again.
exit 1
"""

_FIREFOX_BUILD_CONTENT_UNIX = """\
exports_files(["firefox"])
"""

_FIREFOX_BUILD_CONTENT_WINDOWS = """\
exports_files(["firefox.bat"])

alias(
    name = "firefox",
    actual = "firefox.bat",
    visibility = ["//visibility:public"],
)
"""

def _firefox_local_repository_impl(repository_ctx):
    repository_ctx.file("WORKSPACE.bazel", """workspace(name = "{}")""".format(
        repository_ctx.name,
    ))

    is_windows = False
    if "FIREFOX_BINARY" not in repository_ctx.os.environ:
        script_contents = _FIREFOX_NOT_FOUND_TEMPLATE_UNIX
        if "win" in repository_ctx.os.name:
            is_windows = True
            script_contents = _FIREFOX_NOT_FOUND_TEMPLATE_WINDOWS
    else:
        firefox_bin = repository_ctx.os.environ["FIREFOX_BINARY"]
        template = _FIREFOX_WRAPPER_TEMPLATE_UNIX
        if firefox_bin.endswith((".exe", ".bat")):
            is_windows = True
            template = _FIREFOX_WRAPPER_TEMPLATE_WINDOWS
        script_contents = template.format(
            firefox = firefox_bin,
        )

    repository_ctx.file(
        "firefox{}".format(".bat" if is_windows else ""),
        script_contents,
        executable = True,
    )

    repository_ctx.file("BUILD.bazel", _FIREFOX_BUILD_CONTENT_WINDOWS if is_windows else _FIREFOX_BUILD_CONTENT_UNIX)

firefox_local_repository = repository_rule(
    doc = """\
A repository rule for wrapping the path to a host installed firefox binary

Note that firefox binaries can be found here: https://ftp.mozilla.org/pub/firefox/releases/

However, for platforms like MacOS and Windows, the storage formats are not something that can be extracted
in a repository rule.
""",
    implementation = _firefox_local_repository_impl,
    environ = ["FIREFOX_BINARY"],
)

def firefox_deps():
    """Download firefix/geckodriver dependencies

    Returns:
        A list of repositories crated
    """

    geckodriver_version = "0.35.0"

    direct_deps = []
    for platform, integrity in {
        "linux-aarch64": "sha256-kdHkRmRtjuhYMJcORIBlK3JfGefsvvo//TlHvHviOkc=",
        "linux64": "sha256-rCbpuo87jOD79zObnJAgGS9tz8vwSivNKvgN/muyQmA=",
        "macos": "sha256-zP9gaFH9hNMKhk5LvANTVSOkA4v5qeeHowgXqHdvraE=",
        "macos-aarch64": "sha256-K4XNwwaSsz0nP18Zmj3Q9kc9JXeNlmncVwQmCzm99Xg=",
        "win64": "sha256-5t4e5JqtKUMfe4/zZvEEhtAI3VzY3elMsB1+nj0z2Yg=",
    }.items():
        archive = "tar.gz"
        tool = "geckodriver"
        if "win" in platform:
            archive = "zip"
            tool = "geckodriver.exe"

        name = "geckodriver_{}".format(platform.replace("-", "_"))
        direct_deps.append(struct(repo = name))
        maybe(
            webdriver_repository,
            name = name,
            urls = ["https://github.com/mozilla/geckodriver/releases/download/v{version}/geckodriver-v{version}-{platform}.{archive}".format(
                version = geckodriver_version,
                platform = platform,
                archive = archive,
            )],
            integrity = integrity,
            tool = tool,
        )

    direct_deps.append(struct(repo = "geckodriver"))
    maybe(
        build_file_repository,
        name = "geckodriver",
        build_file = Label("//private/webdrivers/firefox:BUILD.geckodriver.bazel"),
    )

    firefox_version = "136.0"

    for platform, integrity in {
        "linux-aarch64": "sha256-vveh8MLGr9pl8cHtvj4T/dk1wzaxYkMMfTUTkidAgAo=",
        "linux-x86_64": "sha256-UiL1HKrPzK8PDPeVEX8K03Qi/p1BPvGPLBceFiK5RVo=",
        "mac": "sha256-B4VZozSRt8XvXc3mL+PIEoNarpi2On4ys79+M8sz/Mg=",
    }.items():
        archive = "tar.xz"
        firefox_name = "firefox"
        dash = "-"
        strip_prefix = "firefox"
        tool = "firefox"
        if "mac" in platform:
            archive = "dmg"
            firefox_name = "Firefox"
            dash = "%20"
            strip_prefix = ""
            tool = "Firefox.app/Contents/MacOS/firefox"
        name = "firefox_{}".format(platform.replace("-", "_"))
        direct_deps.append(struct(repo = name))
        maybe(
            webdriver_repository,
            name = name,
            urls = ["https://ftp.mozilla.org/pub/firefox/releases/{version}/{platform}/en-US/{name}{dash}{version}.{archive}".format(
                name = firefox_name,
                dash = dash,
                version = firefox_version,
                platform = platform,
                archive = archive,
            )],
            strip_prefix = strip_prefix,
            integrity = integrity,
            tool = tool,
        )

    direct_deps.append(struct(repo = "firefox_local"))
    maybe(
        firefox_local_repository,
        name = "firefox_local",
    )

    direct_deps.append(struct(repo = "firefox"))
    maybe(
        build_file_repository,
        name = "firefox",
        build_file = Label("//private/webdrivers/firefox:BUILD.firefox.bazel"),
    )

    return direct_deps
