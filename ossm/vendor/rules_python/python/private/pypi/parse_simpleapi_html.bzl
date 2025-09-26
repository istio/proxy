# Copyright 2024 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Parse SimpleAPI HTML in Starlark.
"""

def parse_simpleapi_html(*, url, content):
    """Get the package URLs for given shas by parsing the Simple API HTML.

    Args:
        url(str): The URL that the HTML content can be downloaded from.
        content(str): The Simple API HTML content.

    Returns:
        A list of structs with:
        * filename: The filename of the artifact.
        * version: The version of the artifact.
        * url: The URL to download the artifact.
        * sha256: The sha256 of the artifact.
        * metadata_sha256: The whl METADATA sha256 if we can download it. If this is
          present, then the 'metadata_url' is also present. Defaults to "".
        * metadata_url: The URL for the METADATA if we can download it. Defaults to "".
    """
    sdists = {}
    whls = {}
    lines = content.split("<a href=\"")

    _, _, api_version = lines[0].partition("name=\"pypi:repository-version\" content=\"")
    api_version, _, _ = api_version.partition("\"")

    # We must assume the 1.0 if it is not present
    # See https://packaging.python.org/en/latest/specifications/simple-repository-api/#clients
    api_version = api_version or "1.0"
    api_version = tuple([int(i) for i in api_version.split(".")])

    if api_version >= (2, 0):
        # We don't expect to have version 2.0 here, but have this check in place just in case.
        # https://packaging.python.org/en/latest/specifications/simple-repository-api/#versioning-pypi-s-simple-api
        fail("Unsupported API version: {}".format(api_version))

    # Each line follows the following pattern
    # <a href="https://...#sha256=..." attribute1="foo" ... attributeN="bar">filename</a><br />
    sha256s_by_version = {}
    for line in lines[1:]:
        dist_url, _, tail = line.partition("#sha256=")
        dist_url = _absolute_url(url, dist_url)

        sha256, _, tail = tail.partition("\"")

        # See https://packaging.python.org/en/latest/specifications/simple-repository-api/#adding-yank-support-to-the-simple-api
        yanked = "data-yanked" in line

        head, _, _ = tail.rpartition("</a>")
        maybe_metadata, _, filename = head.rpartition(">")
        version = _version(filename)
        sha256s_by_version.setdefault(version, []).append(sha256)

        metadata_sha256 = ""
        metadata_url = ""
        for metadata_marker in ["data-core-metadata", "data-dist-info-metadata"]:
            metadata_marker = metadata_marker + "=\"sha256="
            if metadata_marker in maybe_metadata:
                # Implement https://peps.python.org/pep-0714/
                _, _, tail = maybe_metadata.partition(metadata_marker)
                metadata_sha256, _, _ = tail.partition("\"")
                metadata_url = dist_url + ".metadata"
                break

        if filename.endswith(".whl"):
            whls[sha256] = struct(
                filename = filename,
                version = version,
                url = dist_url,
                sha256 = sha256,
                metadata_sha256 = metadata_sha256,
                metadata_url = _absolute_url(url, metadata_url) if metadata_url else "",
                yanked = yanked,
            )
        else:
            sdists[sha256] = struct(
                filename = filename,
                version = version,
                url = dist_url,
                sha256 = sha256,
                metadata_sha256 = "",
                metadata_url = "",
                yanked = yanked,
            )

    return struct(
        sdists = sdists,
        whls = whls,
        sha256s_by_version = sha256s_by_version,
    )

_SDIST_EXTS = [
    ".tar",  # handles any compression
    ".zip",
]

def _version(filename):
    # See https://packaging.python.org/en/latest/specifications/binary-distribution-format/#binary-distribution-format

    _, _, tail = filename.partition("-")
    version, _, _ = tail.partition("-")
    if version != tail:
        # The format is {name}-{version}-{whl_specifiers}.whl
        return version

    # NOTE @aignas 2025-03-29: most of the files are wheels, so this is not the common path

    # {name}-{version}.{ext}
    for ext in _SDIST_EXTS:
        version, _, _ = version.partition(ext)  # build or name

    return version

def _get_root_directory(url):
    scheme_end = url.find("://")
    if scheme_end == -1:
        fail("Invalid URL format")

    scheme = url[:scheme_end]
    host_end = url.find("/", scheme_end + 3)
    if host_end == -1:
        host_end = len(url)
    host = url[scheme_end + 3:host_end]

    return "{}://{}".format(scheme, host)

def _is_downloadable(url):
    """Checks if the URL would be accepted by the Bazel downloader.

    This is based on Bazel's HttpUtils::isUrlSupportedByDownloader
    """
    return url.startswith("http://") or url.startswith("https://") or url.startswith("file://")

def _absolute_url(index_url, candidate):
    if candidate == "":
        return candidate

    if _is_downloadable(candidate):
        return candidate

    if candidate.startswith("/"):
        # absolute path
        root_directory = _get_root_directory(index_url)
        return "{}{}".format(root_directory, candidate)

    if candidate.startswith(".."):
        # relative path with up references
        candidate_parts = candidate.split("..")
        last = candidate_parts[-1]
        for _ in range(len(candidate_parts) - 1):
            index_url, _, _ = index_url.rstrip("/").rpartition("/")

        return "{}/{}".format(index_url, last.strip("/"))

    # relative path without up-references
    return "{}/{}".format(index_url.rstrip("/"), candidate)
