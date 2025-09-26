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

load("//private/lib:coordinates.bzl", "to_external_form")

_REQUIRED_KEYS = ["artifacts", "dependencies", "repositories"]

def _is_valid_lock_file(lock_file_contents):
    version = lock_file_contents.get("version")
    if "2" != version:
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

def _compute_lock_file_hash(lock_file_contents):
    to_hash = {}
    for key in sorted(_REQUIRED_KEYS):
        value = lock_file_contents.get(key)

        # Encode and de-encode to ensure sorting. This isn't terribly efficient
        # but the json encoding is done natively and is pretty rapid
        to_hash.update({key: json.decode(json.encode(value))})
    return hash(repr(to_hash))

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

def _to_key(unpacked):
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
            key = _to_key(root_unpacked)

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
        "  \"__INPUT_ARTIFACTS_HASH\": %s," % input_hash,
        "  \"__RESOLVED_ARTIFACTS_HASH\": %s," % _compute_lock_file_hash(lock_file_contents),
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
    contents.append("  \"version\": \"2\"")
    contents.append("}")
    contents.append("")

    return "\n".join(contents)

v2_lock_file = struct(
    is_valid_lock_file = _is_valid_lock_file,
    get_input_artifacts_hash = _get_input_artifacts_hash,
    get_lock_file_hash = _get_lock_file_hash,
    compute_lock_file_hash = _compute_lock_file_hash,
    get_artifacts = _get_artifacts,
    get_netrc_entries = _get_netrc_entries,
    render_lock_file = _render_lock_file,
    has_m2local = _has_m2local,
)
