# Copyright 2017 The Bazel Authors. All rights reserved.
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

workspace(name = "rules_python")

# Everything below this line is used only for developing rules_python. Users
# should not copy it to their WORKSPACE.

# Necessary so that Bazel 9 recognizes this as rules_python and doesn't try
# to load the version Bazel itself uses by default.
# buildifier: disable=duplicated-name
local_repository(
    name = "rules_python",
    path = ".",
)

load("//:internal_dev_deps.bzl", "rules_python_internal_deps")

rules_python_internal_deps()

load("@rules_java//java:rules_java_deps.bzl", "rules_java_dependencies")

rules_java_dependencies()

# note that the following line is what is minimally required from protobuf for the java rules
# consider using the protobuf_deps() public API from @com_google_protobuf//:protobuf_deps.bzl
load("@com_google_protobuf//bazel/private:proto_bazel_features.bzl", "proto_bazel_features")  # buildifier: disable=bzl-visibility

proto_bazel_features(name = "proto_bazel_features")

# register toolchains
load("@rules_java//java:repositories.bzl", "rules_java_toolchains")

rules_java_toolchains()

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

load("@rules_jvm_external//:repositories.bzl", "rules_jvm_external_deps")

rules_jvm_external_deps()

load("@rules_jvm_external//:setup.bzl", "rules_jvm_external_setup")

rules_jvm_external_setup()

load("@io_bazel_stardoc//:deps.bzl", "stardoc_external_deps")

stardoc_external_deps()

load("@stardoc_maven//:defs.bzl", stardoc_pinned_maven_install = "pinned_maven_install")

stardoc_pinned_maven_install()

load("//:internal_dev_setup.bzl", "rules_python_internal_setup")

rules_python_internal_setup()

load("@pythons_hub//:versions.bzl", "PYTHON_VERSIONS")
load("//python:repositories.bzl", "py_repositories", "python_register_multi_toolchains")

py_repositories()

python_register_multi_toolchains(
    name = "python",
    default_version = "3.11",
    # Integration tests verify each version, so register all of them.
    python_versions = PYTHON_VERSIONS,
)

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Used for Bazel CI
http_archive(
    name = "bazelci_rules",
    sha256 = "eca21884e6f66a88c358e580fd67a6b148d30ab57b1680f62a96c00f9bc6a07e",
    strip_prefix = "bazelci_rules-1.0.0",
    url = "https://github.com/bazelbuild/continuous-integration/releases/download/rules-1.0.0/bazelci_rules-1.0.0.tar.gz",
)

load("@bazelci_rules//:rbe_repo.bzl", "rbe_preconfig")

# Creates a default toolchain config for RBE.
# Use this as is if you are using the rbe_ubuntu16_04 container,
# otherwise refer to RBE docs.
rbe_preconfig(
    name = "buildkite_config",
    toolchain = "ubuntu2204",
)

local_repository(
    name = "rules_python_gazelle_plugin",
    path = "gazelle",
)

# The rules_python gazelle extension has some third-party go dependencies
# which we need to fetch in order to compile it.
load("@rules_python_gazelle_plugin//:deps.bzl", _py_gazelle_deps = "gazelle_deps")

# See: https://github.com/bazel-contrib/rules_python/blob/main/gazelle/README.md
# This rule loads and compiles various go dependencies that running gazelle
# for python requirements.
_py_gazelle_deps()

# This interpreter is used for various rules_python dev-time tools
interpreter = "@python_3_11_9_host//:python"

#####################
# Install twine for our own runfiles wheel publishing.
# Eventually we might want to install twine automatically for users too, see:
# https://github.com/bazel-contrib/rules_python/issues/1016.
load("@rules_python//python:pip.bzl", "pip_parse")

pip_parse(
    name = "rules_python_publish_deps",
    python_interpreter_target = interpreter,
    requirements_darwin = "//tools/publish:requirements_darwin.txt",
    requirements_lock = "//tools/publish:requirements_linux.txt",
    requirements_windows = "//tools/publish:requirements_windows.txt",
)

load("@rules_python_publish_deps//:requirements.bzl", "install_deps")

install_deps()

pip_parse(
    name = "pypiserver",
    python_interpreter_target = interpreter,
    requirements_lock = "//examples/wheel:requirements_server.txt",
)

load("@pypiserver//:requirements.bzl", install_pypiserver = "install_deps")

install_pypiserver()

#####################
# Install sphinx for doc generation.

pip_parse(
    name = "dev_pip",
    python_interpreter_target = interpreter,
    requirements_lock = "//docs:requirements.txt",
)

load("@dev_pip//:requirements.bzl", docs_install_deps = "install_deps")

docs_install_deps()

#####################
# Pypi repos for //tests/multi_pypi

pip_parse(
    name = "pypi_alpha",
    python_interpreter_target = interpreter,
    requirements_lock = "//tests/multi_pypi/alpha:requirements.txt",
)

load("@pypi_alpha//:requirements.bzl", pypi_alpha_install_deps = "install_deps")

pypi_alpha_install_deps()

pip_parse(
    name = "pypi_beta",
    python_interpreter_target = interpreter,
    requirements_lock = "//tests/multi_pypi/beta:requirements.txt",
)

load("@pypi_beta//:requirements.bzl", pypi_beta_install_deps = "install_deps")

pypi_beta_install_deps()
