# Copyright 2023 The Bazel Authors. All rights reserved.
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

load("//private/lib:urls.bzl", "extract_netrc_from_auth_url", "remove_auth_from_url")

def _is_valid_lock_file(lock_file_contents):
    dep_tree = lock_file_contents.get("dependency_tree")

    if not dep_tree:
        return False

    # The version should match
    if "0.1.0" != dep_tree.get("version"):
        return False

    # And we need dependencies
    if not dep_tree.get("dependencies"):
        return False

    # At this point, we'll only discover problems as we try and generate
    # the build files.
    return True

def _has_m2local(lock_file_contents):
    return lock_file_contents.get("m2local", False)

def _get_input_artifacts_hash(lock_file_contents):
    dep_tree = lock_file_contents.get("dependency_tree", {})
    return dep_tree.get("__INPUT_ARTIFACTS_HASH")

def _get_lock_file_hash(lock_file_contents):
    dep_tree = lock_file_contents.get("dependency_tree", {})
    return dep_tree.get("__RESOLVED_ARTIFACTS_HASH")

def _print_friendly_hash_difference(old_hash, new_hash):
    if old_hash == new_hash:
        return ""
    return "expected %s and got %s" % (old_hash, new_hash)

# The representation of a Windows path when read from the parsed Coursier JSON
# is delimited by 4 back slashes. Replace them with 1 forward slash.
def _normalize_to_unix_path(path):
    return path.replace("\\", "/")

def _compute_lock_file_hash(lock_file_contents):
    dep_tree = lock_file_contents.get("dependency_tree", {})
    artifacts = dep_tree["dependencies"]

    # A collection of elements from the dependency tree to be sorted and hashed
    # into a signature for maven_install.json.
    signature_inputs = []
    for artifact in artifacts:
        artifact_group = []
        artifact_group.append(artifact["coord"])
        if artifact["file"] != None:
            artifact_group.extend([
                artifact["sha256"],
                _normalize_to_unix_path(artifact["file"]),  # Make sure we represent files in a stable way cross-platform
            ])
            if artifact["url"]:
                artifact_group.append(artifact["url"])
        if len(artifact["dependencies"]) > 0:
            artifact_group.append(",".join(sorted(artifact["dependencies"])))
        signature_inputs.append(":".join(artifact_group))
    return hash(repr(sorted(signature_inputs)))

def create_dependency(dep):
    url = dep.get("url")
    if url:
        urls = [remove_auth_from_url(url)]
        urls.extend([remove_auth_from_url(u) for u in dep.get("mirror_urls", []) if u != url])
    elif dep.get("file"):
        urls = ["file://%s" % dep["file"]]
    else:
        urls = []

    return {
        "coordinates": dep["coord"],
        "file": dep.get("file"),
        "sha256": dep.get("sha256"),
        "deps": dep["directDependencies"],
        "urls": urls,
    }

def _get_artifacts(lock_file_contents):
    dep_tree = lock_file_contents.get("dependency_tree", {})
    raw_deps = dep_tree.get("dependencies", [])

    to_return = []
    for dep in raw_deps:
        created = create_dependency(dep)

        if created:
            to_return.append(created)

    return to_return

def add_netrc_entries_from_mirror_urls(netrc_entries, mirror_urls):
    """Add a url's auth credentials into a netrc dict of form return[machine][login] = password."""
    for url in mirror_urls:
        entry = extract_netrc_from_auth_url(url)
        if not entry:
            continue
        machine = entry["machine"]
        login = entry["login"]
        password = entry["password"]
        if machine not in netrc_entries:
            netrc_entries[machine] = {}
        if login not in netrc_entries[machine]:
            if netrc_entries[machine]:
                print("Received multiple logins for machine '{}'! Only using '{}'".format(
                    machine,
                    netrc_entries[machine].keys()[0],
                ))
                continue
            netrc_entries[machine][login] = password
        elif netrc_entries[machine][login] != password:
            print("Received different passwords for {}@{}! Only using the first".format(login, machine))
    return netrc_entries

def _get_netrc_entries(lock_file_contents):
    dep_tree = lock_file_contents.get("dependency_tree", {})
    raw_deps = dep_tree.get("dependencies", [])

    netrc_entries = {}
    for dep in raw_deps:
        if dep.get("mirror_urls"):
            netrc_entries = add_netrc_entries_from_mirror_urls(netrc_entries, dep["mirror_urls"])

    return netrc_entries

v1_lock_file = struct(
    is_valid_lock_file = _is_valid_lock_file,
    get_input_artifacts_hash = _get_input_artifacts_hash,
    get_lock_file_hash = _get_lock_file_hash,
    print_friendly_hash_difference = _print_friendly_hash_difference,
    compute_lock_file_hash = _compute_lock_file_hash,
    get_artifacts = _get_artifacts,
    get_netrc_entries = _get_netrc_entries,
    has_m2local = _has_m2local,
)
