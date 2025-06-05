"""Provides shellcheck dependencies on all supported platforms:
- Linux 64-bit and ARM64
- OSX 64-bit
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def _urls(arch, version):
    url = "https://github.com/vscode-shellcheck/shellcheck-binaries/releases/download/{version}/shellcheck-{version}.{arch}.tar.gz".format(
        version = version,
        arch = arch.replace("_", ".", 1),
    )

    return [
        url.replace("https://", "https://mirror.bazel.build/"),
        url,
    ]

def shellcheck_dependencies():
    version = "v0.9.0"
    sha256 = {
        "darwin_aarch64": "a75b912015aaa5b2a48698b63f3619783d90abda4d32a31362209315e6c1cdf6",
        "darwin_x86_64": "d1244da2aa5d0c2874f3a4a680c6ac79a488ff6dbf9928e12dc80ff3fdc294db",
        "linux_aarch64": "b5633bd195cfe61a310bd8dcff2514855afefea908942a0fd4d01fa6451cb4e6",
        "linux_armv6hf": "4791d36d84a626c4366746d14ad68daf2c07f502da09319c45fa6c5c0a847aa9",
        "linux_x86_64": "0ab5711861e6fcafad5aa21587ee75bbd2b16505d56f41c9ba1191a83d314074",
        "windows_x86_64": "a0f021057b6d6a69a22f6b0db0187bcaca3f5195385e92a7555ad63a6e39ee15",
    }

    for arch, sha256 in sha256.items():
        maybe(
            http_archive,
            name = "shellcheck_{arch}".format(arch = arch),
            build_file_content = """exports_files(["shellcheck"])
""",
            sha256 = sha256,
            urls = _urls(arch = arch, version = version),
        )
