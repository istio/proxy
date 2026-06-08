# Copyright 2024 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Helper functions to register remote jdk repos"""

visibility(["//test"])

_RELEASE_CONFIGS = {
    "8": {
        "zulu": {
            "release": "8.78.0.19-ca-jdk8.0.412",
            "platforms": {
                "linux": ["aarch64", "x86_64"],
                "macos": ["aarch64", "x86_64"],
                "windows": ["x86_64"],
            },
        },
        "adoptopenjdk": {
            "release": "8u292-b10",
            "platforms": {
                "linux": ["s390x"],
            },
        },
    },
    "11": {
        "zulu": {
            "release": "11.72.19-ca-jdk11.0.23",
            "platforms": {
                "linux": ["aarch64", "x86_64"],
                "macos": ["aarch64", "x86_64"],
                "windows": ["x86_64"],
            },
        },
        "adoptium": {
            "release": "11.0.15+10",
            "platforms": {
                "linux": ["ppc", "s390x"],
            },
        },
        "microsoft": {
            "release": "11.0.13.8.1",
            "platforms": {
                "windows": ["arm64"],
            },
        },
    },
    "17": {
        "zulu": {
            "release": "17.50.19-ca-jdk17.0.11",
            "platforms": {
                "linux": ["aarch64", "x86_64"],
                "macos": ["aarch64", "x86_64"],
                "windows": ["arm64", "x86_64"],
            },
        },
        "adoptium": {
            "release": "17.0.8.1+1",
            "platforms": {
                "linux": ["ppc", "s390x"],
            },
        },
    },
    "21": {
        "zulu": {
            "release": "21.36.17-ca-jdk21.0.4",
            "platforms": {
                "linux": ["aarch64", "x86_64"],
                "macos": ["aarch64", "x86_64"],
                "windows": ["arm64", "x86_64"],
            },
        },
        "adoptium": {
            "release": "21.0.4+7",
            "platforms": {
                "linux": ["ppc", "riscv64", "s390x"],
            },
        },
    },
}

_STRIP_PREFIX_OVERRIDES = {
    "remotejdk11_win_arm64": "jdk-11.0.13+8",
}

def _name_for_remote_jdk(version, os, cpu):
    prefix = "remote_jdk" if version == "8" else "remotejdk"
    os_part = "win" if (os == "windows" and version != "8") else os
    if cpu == "x86_64":
        suffix = ""
    elif cpu == "ppc":
        suffix = "_ppc64le"
    else:
        suffix = "_" + cpu
    return prefix + version + "_" + os_part + suffix

def _zulu_remote_jdk_repo(os, cpu, release):
    arch = cpu
    if cpu == "x86_64":
        arch = "x64"
    platform = os
    ext = ".tar.gz"
    if os == "macos":
        platform = "macosx"
    elif os == "windows":
        ext = ".zip"
        platform = "win"
        arch = "aarch64" if arch == "arm64" else arch
    archive_name = "zulu" + release + "-" + platform + "_" + arch
    primary_url = "cdn.azul.com/zulu/bin/" + archive_name + ext
    urls = [
        "https://" + primary_url,
        "https://mirror.bazel.build/" + primary_url,
    ]
    return urls, archive_name

def _adoptium_linux_remote_jdk_repo(version, cpu, release):
    os = "linux"
    arch = cpu
    if cpu == "ppc":
        arch = "ppc64le"
    archive_name = "OpenJDK" + version + "U-jdk_" + arch + "_" + os + "_hotspot_" + release.replace("+", "_") + ".tar.gz"
    primary_url = "github.com/adoptium/temurin" + version + "-binaries/releases/download/jdk-" + release + "/" + archive_name
    urls = [
        "https://" + primary_url,
        "https://mirror.bazel.build/" + primary_url,
    ]
    return urls, "jdk-" + release

def _microsoft_windows_arm64_remote_jdk_repo(release):
    primary_url = "aka.ms/download-jdk/microsoft-jdk-" + release + "-windows-aarch64.zip"
    urls = [
        "https://" + primary_url,
        "https://mirror.bazel.build/" + primary_url,
    ]
    return urls, ""

def _adoptopenjdk_remote_jdk_repo(version, os, cpu, release):
    archive = "OpenJDK" + version + "U-jdk_" + cpu + "_" + os + "_hotspot_" + release.replace("-", "") + ".tar.gz"
    primary_url = "github.com/AdoptOpenJDK/openjdk" + version + "-binaries/releases/download/jdk" + release + "/" + archive
    urls = [
        "https://" + primary_url,
        "https://mirror.bazel.build/" + primary_url,
    ]
    return urls, "jdk" + release

def _flatten_configs():
    result = []
    for version, all_for_version in _RELEASE_CONFIGS.items():
        for distrib, distrib_cfg in all_for_version.items():
            release = distrib_cfg["release"]
            for os, cpus in distrib_cfg["platforms"].items():
                for cpu in cpus:
                    name = _name_for_remote_jdk(version, os, cpu)
                    if distrib == "zulu":
                        urls, strip_prefix = _zulu_remote_jdk_repo(os, cpu, release)
                    elif distrib == "adoptium":
                        if os != "linux":
                            fail("adoptium jdk configured but not linux")
                        urls, strip_prefix = _adoptium_linux_remote_jdk_repo(version, cpu, release)
                    elif distrib == "microsoft":
                        if os != "windows" or cpu != "arm64":
                            fail("only windows_arm64 config for microsoft is expected")
                        urls, strip_prefix = _microsoft_windows_arm64_remote_jdk_repo(release)
                    elif distrib == "adoptopenjdk":
                        urls, strip_prefix = _adoptopenjdk_remote_jdk_repo(version, os, cpu, release)
                    else:
                        fail("unexpected distribution:", distrib)
                    result.append(struct(
                        name = name,
                        version = version,
                        urls = urls,
                        strip_prefix = _STRIP_PREFIX_OVERRIDES.get(name, strip_prefix),
                        target_compatible_with = ["@platforms//os:" + os, "@platforms//cpu:" + cpu],
                    ))
    return result

FLAT_CONFIGS = _flatten_configs()
