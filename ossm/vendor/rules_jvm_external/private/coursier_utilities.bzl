# Copyright 2019 The Bazel Authors. All rights reserved.
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

# Coursier uses these types to determine what files it should resolve and fetch.
# For example, some jars have the type "eclipse-plugin", and Coursier would not
# download them if it's not asked to to resolve "eclipse-plugin".

# Do not load from external dependencies since this is called from the `repositories.bzl` file
# TODO: lift this restriction once we drop workspace-based build support

load("//private/lib:coordinates.bzl", "unpack_coordinates", _SUPPORTED_PACKAGING_TYPES = "SUPPORTED_PACKAGING_TYPES")

SUPPORTED_PACKAGING_TYPES = _SUPPORTED_PACKAGING_TYPES

# See https://github.com/bazelbuild/rules_jvm_external/issues/686
# A single package uses classifier to distinguish the jar files (one per platform),
# So we need to check these are not dependencies of each other.
PLATFORM_CLASSIFIER = [
    "linux-aarch_64",
    "linux-x86_64",
    "osx-aarch_64",
    "osx-x86_64",
    "windows-x86_64",
]

# Lifted from bazel-skylib
def struct_to_dict(s):
    return {
        key: getattr(s, key)
        for key in dir(s)
        if key != "to_json" and key != "to_proto"
    }

def to_repository_name(coords):
    """Converts `coords` into group:artifact[:packaging[:classifier]]:version"""
    unpacked = unpack_coordinates(coords)

    to_return = "%s:%s" % (unpacked.group, unpacked.artifact)

    if unpacked.packaging:
        to_return += ":%s" % unpacked.packaging

    if unpacked.classifier:
        to_return += ":%s" % unpacked.classifier

    if unpacked.version:
        to_return += ":%s" % unpacked.version

    return escape(to_return)

def _to_legacy_format(coord, include_version):
    unpacked = unpack_coordinates(coord)

    to_return = "%s:%s" % (unpacked.group, unpacked.artifact)

    # Note that although "pom" is not a packaging type that Coursier CLI accepts,
    # it's included in the `SUPPORTED_PACKAGING_TYPES` array so we don't need to
    # check it here as well.
    if unpacked.packaging and not unpacked.packaging in SUPPORTED_PACKAGING_TYPES:
        to_return += ":%s" % unpacked.packaging

    if unpacked.classifier and not unpacked.classifier in ["sources", "natives"]:
        to_return += ":%s" % unpacked.classifier

    if include_version and unpacked.version:
        to_return += ":%s" % unpacked.version

    return to_return

def strip_packaging_and_classifier(coord):
    # Strip some packaging and classifier values.
    #
    # We are expected to return one of:
    #
    # groupId:artifactId:version
    # groupId:artifactId:packaging:version
    # groupId:artifactId:packaging:classifier:version

    return _to_legacy_format(coord, True)

def strip_packaging_and_classifier_and_version(coord):
    return _to_legacy_format(coord, False)

def match_group_and_artifact(source, target):
    source_coord = unpack_coordinates(source)
    target_coord = unpack_coordinates(target)
    return source_coord.group == target_coord.group and source_coord.artifact == target_coord.artifact

def get_packaging(coord):
    # Get packaging from the following maven coordinate
    return unpack_coordinates(coord).packaging

def get_classifier(coord):
    # Get classifier from the following maven coordinate
    return unpack_coordinates(coord).classifier

def escape(string):
    for char in [".", "-", ":", "/", "+", "$"]:
        string = string.replace(char, "_")
    return string.replace("[", "").replace("]", "").split(",")[0]

def is_maven_local_path(absolute_path):
    # Return whether or not the provided absolute path corresponds to maven local
    return absolute_path and len(absolute_path.split(".m2/repository")) == 2

def contains_git_conflict_markers(file_name, lock_file_content):
    for line in lock_file_content.splitlines():
        if line.startswith("<<<<<<<") or line.startswith(">>>>>>>") or line.startswith("======="):
            # An expected workflow is for people to do:
            #
            # 1. `git pull`
            # 2. Find a conflict in their lock file
            # 3. Run `REPIN=1 bazel run @maven//:pin` to fix the problem
            #
            # Because of this, we don't want to fail the build, but we do want
            # to warn users that something is quite right.
            print("Conflict markers detected in lock file {}. You should reset the file and repin your dependencies".format(file_name))
            return True
    return False
