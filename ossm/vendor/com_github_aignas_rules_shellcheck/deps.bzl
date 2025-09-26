"""Provides shellcheck dependencies on all supported platforms:
- Linux 64-bit and ARM64
- OSX 64-bit
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def _urls(arch, version):
    archive_template_name = {
        "darwin_aarch64": "shellcheck-{version}.{arch}.tar.xz",
        "darwin_x86_64": "shellcheck-{version}.{arch}.tar.xz",
        "linux_aarch64": "shellcheck-{version}.{arch}.tar.xz",
        "linux_armv6hf": "shellcheck-{version}.{arch}.tar.xz",
        "linux_x86_64": "shellcheck-{version}.{arch}.tar.xz",
        "windows_x86_64": "shellcheck-{version}.zip",
    }
    url = "https://github.com/koalaman/shellcheck/releases/download/{version}/{archive}".format(
        version = version,
        archive = archive_template_name[arch].format(
            version = version,
            arch = arch.replace("_", ".", 1),
        )
    )

    return [
        url,
    ]

def shellcheck_dependencies():
    version = "v0.11.0"
    sha256 = {
        "darwin_aarch64": "56affdd8de5527894dca6dc3d7e0a99a873b0f004d7aabc30ae407d3f48b0a79",
        "darwin_x86_64": "3c89db4edcab7cf1c27bff178882e0f6f27f7afdf54e859fa041fca10febe4c6",
        "linux_aarch64": "12b331c1d2db6b9eb13cfca64306b1b157a86eb69db83023e261eaa7e7c14588",
        "linux_armv6hf": "8afc50b302d5feeac9381ea114d563f0150d061520042b254d6eb715797c8223",
        "linux_x86_64": "8c3be12b05d5c177a04c29e3c78ce89ac86f1595681cab149b65b97c4e227198",
    }

    for arch, sha256 in sha256.items():
        maybe(
            http_archive,
            name = "shellcheck_{arch}".format(arch = arch),
            strip_prefix = "shellcheck-{version}".format(version = version),
            build_file_content = """exports_files(["shellcheck"])
""",
            sha256 = sha256,
            urls = _urls(arch = arch, version = version),
        )

    # Special case, as it is a zip archive with no prefix to strip.
    maybe(
        http_archive,
        name = "shellcheck_windows_x86_64",
        build_file_content = """exports_files(["shellcheck"])
""",
        sha256 = "8a4e35ab0b331c85d73567b12f2a444df187f483e5079ceffa6bda1faa2e740e",
        urls = _urls(arch = "windows_x86_64", version = version),
    )
