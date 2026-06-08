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

load("//private/lib:coordinates.bzl", "to_external_form", "to_key")

_REQUIRED_KEYS = ["artifacts", "dependencies", "repositories"]

def _is_valid_lock_file_v2(lock_file_contents):
    return _is_valid_lock_file(lock_file_contents, "2")

def _is_valid_lock_file_v3(lock_file_contents):
    return _is_valid_lock_file(lock_file_contents, "3")

def _is_valid_lock_file(lock_file_contents, desired_version):
    version = lock_file_contents.get("version")
    if desired_version != version:
        return False

    all_keys = lock_file_contents.keys()

    for key in _REQUIRED_KEYS:
        if key not in all_keys:
            return False

    return True

def _has_m2local(lock_file_contents):
    return lock_file_contents.get("m2local", False)

def _get_input_artifacts_hash(lock_file_contents):
    return lock_file_contents.get("__INPUT_ARTIFACTS_HASH")

def _get_lock_file_hash(lock_file_contents):
    return lock_file_contents.get("__RESOLVED_ARTIFACTS_HASH")

def _print_friendly_hash_difference_v2(old_hash, new_hash):
    if old_hash == new_hash:
        return ""
    return "expected %s and got %s" % (old_hash, new_hash)

def _print_friendly_hash_difference_v3(old_hash, new_hash):
    if old_hash == new_hash:
        return ""

    differences = []

    all_keys = {k: True for k in list(old_hash.keys()) + list(new_hash.keys())}

    for key in sorted(all_keys.keys()):
        old_val = old_hash.get(key)
        new_val = new_hash.get(key)

        if old_val != new_val:
            if old_val == None:
                differences.append("%s: added" % key)
            elif new_val == None:
                differences.append("%s: removed" % key)
            else:
                differences.append("%s: %s vs %s" % (key, old_val, new_val))

    total_count = len(differences)
    shown = differences[:3]
    other_count = total_count - len(shown)

    result = "changes: " + ", ".join(shown)
    if other_count == 1:
        result += ", and 1 other change"
    elif other_count > 1:
        result += ", and %d other changes" % other_count
    return result

def _compute_lock_file_hash_v2(lock_file_contents):
    to_hash = {}
    for key in sorted(_REQUIRED_KEYS):
        value = lock_file_contents.get(key)

        # Encode and de-encode to ensure sorting. This isn't terribly efficient
        # but the json encoding is done natively and is pretty rapid
        to_hash.update({key: json.decode(json.encode(value))})
    return hash(repr(to_hash))

def _compute_final_hash(all_infos):
    final_hashes = dict()

    # in case of circular dependencies, we take a normal hash of the original info as a starting point
    backup_hashes = {k: hash(repr(v)) for k, v in all_infos.items()}

    # sets are bazel 8 only, we use a dict instead
    remaining = {k: 0 for k in all_infos.keys()}

    # bazel does not support recursion, we have to emulate it manually
    stack = []

    # replacement for while True
    for _ in range(1000000000):
        if len(remaining) == 0 and len(stack) == 0:
            break

        curr = None
        if len(stack) == 0:
            curr, _ = remaining.popitem()
        else:
            curr = stack.pop()

        if curr in final_hashes:
            continue

        deps = all_infos[curr].get("dependencies", [])

        # make sure all dependencies are processed first
        unprocessed = [d for d in deps if d in remaining]
        if len(unprocessed) > 0:
            dep = unprocessed[0]
            stack.append(curr)
            stack.append(dep)
            remaining.pop(dep, None)
            continue

        all_infos[curr]["dependency_hashes"] = {dep: final_hashes.get(dep, backup_hashes.get(dep, 0)) for dep in deps}
        final_hashes[curr] = hash(repr(all_infos[curr]))

    return final_hashes

def _compute_lock_file_hash_v3(lock_file_contents):
    all_infos = dict()

    for dep, dep_info in lock_file_contents["artifacts"].items():
        shasums = dep_info["shasums"]
        common_info = {k: v for k, v in dep_info.items() if k != "shasums"}

        is_jar_type = dep.count(":") == 1

        for type, sha in shasums.items():
            jar_suffix = ":jar" if is_jar_type else ""
            suffix = jar_suffix + ":" + type if type != "jar" else ""

            type_info = dict()
            type_info["standard"] = common_info
            type_info["sha"] = sha
            all_infos[dep + suffix] = type_info

    for repo, artifacts in lock_file_contents["repositories"].items():
        for artifact in artifacts:
            all_infos[artifact]["repository"] = repo

    for dep, dep_info in lock_file_contents["dependencies"].items():
        all_infos[dep]["dependencies"] = sorted(dep_info)

    return _compute_final_hash(all_infos)

def _to_m2_path(unpacked):
    path = "{group}/{artifact}/{version}/{artifact}-{version}".format(
        artifact = unpacked["artifact"],
        group = unpacked["group"].replace(".", "/"),
        version = unpacked["version"],
    )

    classifier = unpacked.get("classifier", "jar")
    if not classifier:
        classifier = "jar"
    if "jar" != classifier:
        path += "-%s" % classifier

    extension = unpacked.get("packaging", "jar")
    if not extension:
        extension = "jar"
    path += ".%s" % extension

    return path

def _to_maven_coordinates(unpacked):
    coords = "%s:%s" % (unpacked["group"], unpacked["artifact"])

    extension = unpacked.get("packaging", "jar")
    if not extension:
        extension = "jar"
    classifier = unpacked.get("classifier", "jar")
    if not classifier:
        classifier = "jar"

    if classifier != "jar":
        coords += ":%s:%s" % (extension, classifier)
    elif extension != "jar":
        coords += ":%s" % extension
    coords += ":%s" % unpacked.get("version")

    return coords

def _from_key(key, spoofed_version):
    expected = "%s:%s" % (key, spoofed_version)

    parts = key.split(":")

    # group:artifact[:packaging[:classifier]]:version
    # group:artifact[:version][:classifier][@packaging]
    to_return = "%s:%s:%s" % (parts[0], parts[1], spoofed_version)
    if len(parts) == 4:
        to_return += ":%s@%s" % (parts[3], parts[2])
    elif len(parts) == 3:
        to_return += "@%s" % (parts[2])

    return to_return

def _get_artifacts(lock_file_contents):
    raw_artifacts = lock_file_contents.get("artifacts", {})
    dependencies = lock_file_contents.get("dependencies", {})
    repositories = lock_file_contents.get("repositories", {})
    files = lock_file_contents.get("files", {})
    skipped = lock_file_contents.get("skipped", [])
    services = lock_file_contents.get("services", {})

    artifacts = []

    for (root, data) in raw_artifacts.items():
        # The `root` is `group:artifact[:extension]`. We know the classifiers
        # we saw for this particular coordinate because each classifier is a key in
        # `data["shasums"]`. `data["version"]` gives us the version number. From this
        # information we can reconstruct each of the coordinates that we want to use.
        parts = root.split(":")

        root_unpacked = {
            "group": parts[0],
            "artifact": parts[1],
            "version": data["version"],
        }
        if len(parts) > 2:
            root_unpacked["packaging"] = parts[2]
        else:
            root_unpacked["packaging"] = "jar"

        for (classifier, shasum) in data.get("shasums", {}).items():
            root_unpacked["classifier"] = classifier
            coordinates = to_external_form(root_unpacked)
            key = to_key(root_unpacked)

            urls = []
            for (repo, artifacts_within_repo) in repositories.items():
                if key in artifacts_within_repo:
                    urls.append("%s%s" % (repo, _to_m2_path(root_unpacked)))

            if key in skipped:
                file = None
            elif files.get(key):
                file = files[key]
            else:
                file = _to_m2_path(root_unpacked)

            # Deps originally had a version number, but now they're stripped of that
            # after we moved to this lock file format. However, all the code in the
            # rest of the repo assumes that the deps will be have them. Since we don't
            # expect those deps to matter, fake it.
            deps = [_from_key(dep, "spoofed-version") for dep in dependencies.get(key, [])]

            artifacts.append({
                "coordinates": coordinates,
                "sha256": shasum,
                "file": file,
                "deps": deps,
                "annotation_processors": services.get(root, {}).get("javax.annotation.processing.Processor", []),
                "urls": urls,
            })

    return artifacts

def _get_netrc_entries(lock_file_contents):
    return {}

def _render_lock_file(lock_file_contents, input_hash):
    # We would like to use `json.encode_indent` but that sorts dictionaries, and
    # we've carefully preserved ordering of the repositories. We need to handle
    # this ourselves.
    contents = [
        "{",
        "  \"__AUTOGENERATED_FILE_DO_NOT_MODIFY_THIS_FILE_MANUALLY\": \"THERE_IS_NO_DATA_ONLY_ZUUL\",",
        "  \"__INPUT_ARTIFACTS_HASH\": %s," % json.encode_indent(input_hash, prefix = "  ", indent = "  "),
        "  \"__RESOLVED_ARTIFACTS_HASH\": %s," % json.encode_indent(_compute_lock_file_hash_v3(lock_file_contents), prefix = "  ", indent = "  "),
    ]
    if lock_file_contents.get("conflict_resolution"):
        contents.append("  \"conflict_resolution\": %s," % json.encode_indent(lock_file_contents["conflict_resolution"], prefix = "  ", indent = "  "))
    contents.append("  \"artifacts\": %s," % json.encode_indent(lock_file_contents["artifacts"], prefix = "  ", indent = "  "))
    contents.append("  \"dependencies\": %s," % json.encode_indent(lock_file_contents["dependencies"], prefix = "  ", indent = "  "))
    if lock_file_contents.get("m2local"):
        contents.append("  \"m2local\": true,")
    contents.append("  \"packages\": %s," % json.encode_indent(lock_file_contents["packages"], prefix = "  ", indent = "  "))
    contents.append("  \"repositories\": {")

    items = lock_file_contents["repositories"].items()
    count = len(items)
    for (repo, artifacts) in items:
        count = count - 1
        to_append = "    \"%s\": %s" % (repo, json.encode_indent(artifacts, prefix = "    ", indent = "  "))
        if count:
            to_append += ","
        contents.append(to_append)
    contents.append("  },")
    contents.append("  \"services\": %s," % json.encode_indent(lock_file_contents["services"], prefix = "  ", indent = "  "))
    if lock_file_contents.get("skipped"):
        contents.append("  \"skipped\": %s," % json.encode_indent(lock_file_contents["skipped"], prefix = "  ", indent = "  "))
    contents.append("  \"version\": \"3\"")
    contents.append("}")
    contents.append("")

    return "\n".join(contents)

v2_lock_file = struct(
    is_valid_lock_file = _is_valid_lock_file_v2,
    get_input_artifacts_hash = _get_input_artifacts_hash,
    get_lock_file_hash = _get_lock_file_hash,
    print_friendly_hash_difference = _print_friendly_hash_difference_v2,
    compute_lock_file_hash = _compute_lock_file_hash_v2,
    get_artifacts = _get_artifacts,
    get_netrc_entries = _get_netrc_entries,
    has_m2local = _has_m2local,
)

v3_lock_file = struct(
    is_valid_lock_file = _is_valid_lock_file_v3,
    get_input_artifacts_hash = _get_input_artifacts_hash,
    get_lock_file_hash = _get_lock_file_hash,
    print_friendly_hash_difference = _print_friendly_hash_difference_v3,
    compute_lock_file_hash = _compute_lock_file_hash_v3,
    get_artifacts = _get_artifacts,
    get_netrc_entries = _get_netrc_entries,
    render_lock_file = _render_lock_file,
    has_m2local = _has_m2local,
)
