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
"""Utilities to help create a rule set release."""

import hashlib
import string
import sys
import textwrap


WORKSPACE_STANZA_TEMPLATE = string.Template(textwrap.dedent(
    """
    load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
    http_archive(
        name = "${repo}",
        urls = [
            ${urls},
        ],
        sha256 = "${sha256}",
    )
    """).strip())


DEPS_STANZA_TEMPLATE = string.Template(textwrap.dedent(
    """
    load("@${repo}//${setup_file}", ${to_load})
    """).strip())


def package_basename(repo, version):
  return '%s-%s.tar.gz' % (repo, version)


def get_package_sha256(tarball_path):
  with open(tarball_path, 'rb') as pkg_content:
    tar_sha256 = hashlib.sha256(pkg_content.read()).hexdigest()
  return tar_sha256


def workspace_content(
    url,
    repo,
    sha256,
    deps_method=None,
    mirror_url=None,
    rename_repo=None,
    setup_file=None,
    toolchains_method=None):
  # Create the WORKSPACE stanza needed for this rule set.
  if setup_file and not (deps_method or toolchains_method):
    print(
        'setup_file can only be set if at least one of (deps_method, toolchains_method) is set.',
        flush=True,
        file=sys.stderr,
    )
    sys.exit(1)

  methods = []
  if deps_method:
    methods.append(deps_method)
  if toolchains_method:
    methods.append(toolchains_method)

  # If the github repo has a '-' in the name, that breaks bazel unless we remove
  # it or change it to an '_'
  repo = rename_repo or repo
  repo = repo.replace('-', '_')
  # Correct the common mistake of not putting a ':' in your setup file name
  if setup_file and ':' not in setup_file:
    setup_file = ':' + setup_file
  if mirror_url:
    # this could be more elegant
    urls = '"%s",\n        "%s"' % (mirror_url, url)
  else:
    urls = '"%s"' % url
  ret = WORKSPACE_STANZA_TEMPLATE.substitute({
      'urls': urls,
      'sha256': sha256,
      'repo': repo,
  })
  if methods:
    deps = DEPS_STANZA_TEMPLATE.substitute({
        'repo': repo,
        'setup_file': setup_file or ':deps.bzl',
        'to_load': ', '.join('"%s"' % m for m in methods),
    })
    ret += '\n%s\n' % deps

  for m in methods:
    ret += '%s()\n' % m

  return ret
