#!/usr/bin/env python3
# Copyright 2022 The Bazel Authors. All rights reserved.
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
"""merge stardoc output into a single page.

- concatenates files
- corrects things that stardoc botches
"""

import re
import sys
import typing


ID_RE = re.compile(r'<a id="(.*)">')
WRAPS_RE = re.compile(r'@wraps\((.*)\)')
SINCE_RE = re.compile(r'@since\(([^)]*)\)')
CENTER_RE = re.compile(r'<p align="center">([^<]*)</p>')


def merge_file(file: str, out, wrapper_map:typing.Dict[str, str]) -> None:
  with open(file, 'r') as inp:
    content = inp.read()
    m = ID_RE.search(content)
    this_pkg = m.group(1) if m else None
    m = WRAPS_RE.search(content)
    if m:
      # I wrap something, so don't emit me.
      wrapper_map[m.group(1)] = this_pkg
      return
    # If something wraps me, rewrite myself with the wrapper name.
    if this_pkg in wrapper_map:
      content = content.replace(this_pkg, wrapper_map[this_pkg])
      del wrapper_map[this_pkg]
    merge_text(content, out)


def merge_text(text: str, out) -> None:
  """Merge a block of text into an output stream.

  Args:
    text: block of text produced by Starroc.
    out: an output file stream.
  """
  for line in text.split('\n'):
    line = SINCE_RE.sub(r'<div class="since"><i>Since \1</i></div>', line)

    if line.startswith('| :'):
      line = fix_stardoc_table_align(line)
    # Compensate for https://github.com/bazelbuild/stardoc/issues/118.
    # Convert escaped HTML <li> back to raw text
    line = line.replace('&lt;li&gt;', '<li>')
    line = CENTER_RE.sub(r'\1', line)
    _ = out.write(line)
    _ = out.write('\n')


def fix_stardoc_table_align(line: str) -> str:
  """Change centered descriptions to left justified."""
  if line.startswith('| :-------------: | :-------------: '):
    return '| :------------ | :--------------- | :---------: | :---------: | :----------- |'
  return line


def main(argv: typing.Sequence[str]) -> None:
  wrapper_map = {}
  for file in argv[1:]:
    merge_file(file, sys.stdout, wrapper_map)
  if wrapper_map:
    print("We didn't use all the @wraps()", wrapper_map, file=sys.stderr)
    sys.exit(1)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
