#!/usr/bin/env bash
# Copyright 2023 The Bazel Authors. All rights reserved.
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

set -o errexit -o nounset -o pipefail

# Exclude dot directories, specifically, this file so that we don't
# find the substring we're looking for in our own file.
# Exclude CONTRIBUTING.md, RELEASING.md because they document how to use these strings.
if grep --exclude=CONTRIBUTING.md --exclude=RELEASING.md --exclude-dir=.* VERSION_NEXT_ -r; then
  echo
  echo "Found VERSION_NEXT markers indicating version needs to be specified"
  exit 1
fi

# Set by GH actions, see
# https://docs.github.com/en/actions/learn-github-actions/environment-variables#default-environment-variables
TAG=${GITHUB_REF_NAME}
# A prefix is added to better match the GitHub generated archives.
PREFIX="rules_python-${TAG}"
ARCHIVE="rules_python-$TAG.tar.gz"
git archive --format=tar --prefix=${PREFIX}/ ${TAG} | gzip > $ARCHIVE
SHA=$(shasum -a 256 $ARCHIVE | awk '{print $1}')

cat > release_notes.txt << EOF

For more detailed setup instructions, see https://rules-python.readthedocs.io/en/latest/getting-started.html

For the user-facing changelog see [here](https://rules-python.readthedocs.io/en/latest/changelog.html#v${TAG//./-})

## Using Bzlmod

Add to your \`MODULE.bazel\` file:

\`\`\`starlark
bazel_dep(name = "rules_python", version = "${TAG}")

python = use_extension("@rules_python//python/extensions:python.bzl", "python")
python.toolchain(
    python_version = "3.13",
)

pip = use_extension("@rules_python//python/extensions:pip.bzl", "pip")
pip.parse(
    hub_name = "pypi",
    python_version = "3.13",
    requirements_lock = "//:requirements_lock.txt",
)

use_repo(pip, "pypi")
\`\`\`

## Using WORKSPACE

Paste this snippet into your \`WORKSPACE\` file:

\`\`\`starlark
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rules_python",
    sha256 = "${SHA}",
    strip_prefix = "${PREFIX}",
    url = "https://github.com/bazel-contrib/rules_python/releases/download/${TAG}/rules_python-${TAG}.tar.gz",
)

load("@rules_python//python:repositories.bzl", "py_repositories")

py_repositories()
\`\`\`

### Gazelle plugin

Paste this snippet into your \`WORKSPACE\` file:

\`\`\`starlark
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
http_archive(
    name = "rules_python_gazelle_plugin",
    sha256 = "${SHA}",
    strip_prefix = "${PREFIX}/gazelle",
    url = "https://github.com/bazel-contrib/rules_python/releases/download/${TAG}/rules_python-${TAG}.tar.gz",
)

# To compile the rules_python gazelle extension from source,
# we must fetch some third-party go dependencies that it uses.

load("@rules_python_gazelle_plugin//:deps.bzl", _py_gazelle_deps = "gazelle_deps")

_py_gazelle_deps()
\`\`\`
EOF
