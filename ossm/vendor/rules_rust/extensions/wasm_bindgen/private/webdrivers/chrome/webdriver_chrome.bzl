"""Depednencies for `wasm_bindgen_test` rules"""

load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("//private/webdrivers:webdriver_utils.bzl", "build_file_repository", "webdriver_repository")

# A snippet from https://googlechromelabs.github.io/chrome-for-testing/known-good-versions-with-downloads.json
# but modified to included `integrity`
CHROME_DATA = {
    "downloads": {
        "chrome": [
            {
                "integrity": "sha256-fm2efJlMZRGqLP7dgfMqhz6CKY/qVJrO6lfDZLLhA/k=",
                "platform": "linux64",
                "url": "https://storage.googleapis.com/chrome-for-testing-public/136.0.7055.0/linux64/chrome-linux64.zip",
            },
            {
                "integrity": "sha256-uROIJ56CjR6ZyjiwPCAB7n21aSoA3Wi6TluPnAch8YM=",
                "platform": "mac-arm64",
                "url": "https://storage.googleapis.com/chrome-for-testing-public/136.0.7055.0/mac-arm64/chrome-mac-arm64.zip",
            },
            {
                "integrity": "sha256-e9rzOOF8n43J5IpwvBQjnKyGc26QnBcTygpALtGDCK0=",
                "platform": "mac-x64",
                "url": "https://storage.googleapis.com/chrome-for-testing-public/136.0.7055.0/mac-x64/chrome-mac-x64.zip",
            },
            {
                "integrity": "sha256-tp67N9Iy0WPqRBqCPG66ow/TTEcBuSuS/XsJwms253Q=",
                "platform": "win32",
                "url": "https://storage.googleapis.com/chrome-for-testing-public/136.0.7055.0/win32/chrome-win32.zip",
            },
            {
                "integrity": "sha256-GhiJkB9FcXIxdIqWf2gsJh0jYLWzx2V2r3wWLRcwSSk=",
                "platform": "win64",
                "url": "https://storage.googleapis.com/chrome-for-testing-public/136.0.7055.0/win64/chrome-win64.zip",
            },
        ],
        "chrome-headless-shell": [
            {
                "integrity": "sha256-ZfahmT+jnxYV7e6e62Fj5W32FPJryOgAmhVDjNDp0Hk=",
                "platform": "linux64",
                "url": "https://storage.googleapis.com/chrome-for-testing-public/136.0.7055.0/linux64/chrome-headless-shell-linux64.zip",
            },
            {
                "integrity": "sha256-Rfdu/e4raKeTCvh5FgK4H6rrAG14KRWK4fAzoOrqUBQ=",
                "platform": "mac-arm64",
                "url": "https://storage.googleapis.com/chrome-for-testing-public/136.0.7055.0/mac-arm64/chrome-headless-shell-mac-arm64.zip",
            },
            {
                "integrity": "sha256-TWHvAfeYDKifKQD95rSantkCtpR3vLKraP41VnlGFmA=",
                "platform": "mac-x64",
                "url": "https://storage.googleapis.com/chrome-for-testing-public/136.0.7055.0/mac-x64/chrome-headless-shell-mac-x64.zip",
            },
            {
                "integrity": "sha256-KuWJUK12L+K4sQwRRecq0qrqz4CLDqPkN3c31vpMLXI=",
                "platform": "win32",
                "url": "https://storage.googleapis.com/chrome-for-testing-public/136.0.7055.0/win32/chrome-headless-shell-win32.zip",
            },
            {
                "integrity": "sha256-9ZoAYNyG2yu/QQLNqVjbBMjrj5WtaSm62Ydp8u4BXqk=",
                "platform": "win64",
                "url": "https://storage.googleapis.com/chrome-for-testing-public/136.0.7055.0/win64/chrome-headless-shell-win64.zip",
            },
        ],
        "chromedriver": [
            {
                "integrity": "sha256-FB1JYbgukDV7/PngapsD7b09XcqO5h01VFtTGPq6has=",
                "platform": "linux64",
                "url": "https://storage.googleapis.com/chrome-for-testing-public/136.0.7055.0/linux64/chromedriver-linux64.zip",
            },
            {
                "integrity": "sha256-ZATjVJV7PsswgvgdT7zQeeC6pgflX6qcrQmHxwY+/xE=",
                "platform": "mac-arm64",
                "url": "https://storage.googleapis.com/chrome-for-testing-public/136.0.7055.0/mac-arm64/chromedriver-mac-arm64.zip",
            },
            {
                "integrity": "sha256-8IsY85x99wA1VtRY5K1vB9hB0tJtzPgfNJaLYEUk7mw=",
                "platform": "mac-x64",
                "url": "https://storage.googleapis.com/chrome-for-testing-public/136.0.7055.0/mac-x64/chromedriver-mac-x64.zip",
            },
            {
                "integrity": "sha256-HYamG2SqvfrCLbxaQIc8CI5x1KnZ/XkJ0Y3RKwmFaDo=",
                "platform": "win32",
                "url": "https://storage.googleapis.com/chrome-for-testing-public/136.0.7055.0/win32/chromedriver-win32.zip",
            },
            {
                "integrity": "sha256-IxvDZCKGBTZkYDT/M7XQOfI/DaQxA9yYPJ0PB5XniVQ=",
                "platform": "win64",
                "url": "https://storage.googleapis.com/chrome-for-testing-public/136.0.7055.0/win64/chromedriver-win64.zip",
            },
        ],
    },
    "revision": "1429446",
    "version": "136.0.7055.0",
}

def chrome_deps():
    """Download chromedriver dependencies

    Returns:
        A list of repositories crated
    """

    direct_deps = []
    for data in CHROME_DATA["downloads"]["chromedriver"]:
        platform = data["platform"]
        name = "chromedriver_{}".format(platform.replace("-", "_"))
        direct_deps.append(struct(repo = name))
        tool = "chromedriver"
        if platform.startswith("win"):
            tool = "chromedriver.exe"
        maybe(
            webdriver_repository,
            name = name,
            urls = [data["url"]],
            strip_prefix = "chromedriver-{}".format(platform),
            integrity = data.get("integrity", ""),
            tool = tool,
        )

    for data in CHROME_DATA["downloads"]["chrome-headless-shell"]:
        platform = data["platform"]
        name = "chrome_headless_shell_{}".format(platform.replace("-", "_"))
        direct_deps.append(struct(repo = name))
        tool = "chrome-headless-shell"
        if platform.startswith("win"):
            tool = "chrome-headless-shell.exe"
        maybe(
            webdriver_repository,
            name = name,
            urls = [data["url"]],
            strip_prefix = "chrome-headless-shell-{}".format(platform),
            integrity = data.get("integrity", ""),
            tool = tool,
        )

    for data in CHROME_DATA["downloads"]["chrome"]:
        platform = data["platform"]
        name = "chrome_{}".format(platform.replace("-", "_"))
        direct_deps.append(struct(repo = name))

        if platform.startswith("win"):
            tool = "chrome.exe"
        elif platform.startswith("mac"):
            tool = "Google Chrome for Testing.app/Contents/MacOS/Google Chrome for Testing"
        else:
            tool = "chrome"
        maybe(
            webdriver_repository,
            name = name,
            urls = [data["url"]],
            strip_prefix = "chrome-{}".format(platform),
            integrity = data.get("integrity", ""),
            tool = tool,
        )

    direct_deps.append(struct(repo = "chromedriver"))
    maybe(
        build_file_repository,
        name = "chromedriver",
        build_file = Label("//private/webdrivers/chrome:BUILD.chromedriver.bazel"),
    )

    direct_deps.append(struct(repo = "chrome_headless_shell"))
    maybe(
        build_file_repository,
        name = "chrome_headless_shell",
        build_file = Label("//private/webdrivers/chrome:BUILD.chrome_headless_shell.bazel"),
    )

    direct_deps.append(struct(repo = "chrome"))
    maybe(
        build_file_repository,
        name = "chrome",
        build_file = Label("//private/webdrivers/chrome:BUILD.chrome.bazel"),
    )

    return direct_deps
