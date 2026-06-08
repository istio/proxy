# Copyright 2021 The Bazel Authors. All rights reserved.
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
"""Utilities to extract git commit descriptions in useful ways."""

import argparse
import os
import subprocess
import sys


def guess_previous_release_tag(git_path, pattern=None):
  assert git_path
  most_recent = None
  cmd = [git_path, 'tag']
  if pattern:
    cmd.extend(['--list', pattern])
  # We are doing something dumb here for now. Grab the list of tags, and pick
  # the last one.
  with subprocess.Popen(cmd, stdout=subprocess.PIPE) as proc:
    most_recent = proc.stdout.read().decode('utf-8')
    most_recent = most_recent.strip().replace('\n\n', '\n').split('\n')[-1]
  return most_recent


def git_changelog(from_ref, to_ref='HEAD', git_path=None):
  assert from_ref
  assert to_ref
  assert git_path
  cmd = [git_path, 'log', '%s..%s' % (from_ref, to_ref)]
  with subprocess.Popen(cmd, stdout=subprocess.PIPE) as proc:
    return proc.stdout.read().decode('utf-8')


def main():
  parser = argparse.ArgumentParser(
      description='Helper for extracting git changelog',
      fromfile_prefix_chars='@')
  parser.add_argument('--git_path', required=True, help='path to git binary')
  parser.add_argument('--git_root', required=True, help='path to git client')
  parser.add_argument('--out', required=True, help='output path')
  parser.add_argument('--from_ref', help='from REF')
  parser.add_argument('--to_ref', help='to REF')
  parser.add_argument('--verbose', action='store_true')

  options = parser.parse_args()

  with open(options.out, 'w', encoding='utf-8') as out:
    os.chdir(options.git_root)
    from_ref = options.from_ref
    if not from_ref or from_ref == '_LATEST_TAG_':
      from_ref = guess_previous_release_tag(options.git_path)
    to_ref = options.to_ref or 'HEAD'
    if options.verbose:
      print('Getting changelog from %s to %s' % (from_ref, to_ref))
    changelog = git_changelog(
        from_ref=from_ref, to_ref=to_ref, git_path=options.git_path)
    out.write(changelog)
  return 0

if __name__ == '__main__':
  main()
