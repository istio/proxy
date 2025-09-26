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
# limitations under the License.

load("//private:constants.bzl", _DEFAULT_REPOSITORY_NAME = "DEFAULT_REPOSITORY_NAME")
load("//private/rules:artifact.bzl", _artifact = "artifact", _java_plugin_artifact = "java_plugin_artifact", _maven_artifact = "maven_artifact")
load("//private/rules:has_maven_deps.bzl", _read_coordinates = "read_coordinates")
load("//private/rules:java_export.bzl", _java_export = "java_export", _maven_export = "maven_export")
load("//private/rules:javadoc.bzl", _javadoc = "javadoc")
load("//private/rules:maven_bom.bzl", _maven_bom = "maven_bom")
load("//private/rules:maven_install.bzl", _maven_install = "maven_install")
load("//private/rules:maven_publish.bzl", _MavenPublishInfo = "MavenPublishInfo")
load("//private/rules:pom_file.bzl", _pom_file = "pom_file")

DEFAULT_REPOSITORY_NAME = _DEFAULT_REPOSITORY_NAME

artifact = _artifact
java_export = _java_export
maven_export = _maven_export
javadoc = _javadoc
java_plugin_artifact = _java_plugin_artifact
maven_artifact = _maven_artifact
maven_bom = _maven_bom
maven_install = _maven_install
pom_file = _pom_file
read_coordinates = _read_coordinates
MavenPublishInfo = _MavenPublishInfo
