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
"""Compare to content manifest files."""

import json
import unittest

from python.runfiles import runfiles

class ContentManifestTest(unittest.TestCase):
  """Test harness to see if we wrote the content manifest correctly."""

  run_files = runfiles.Create()

  def assertManifestsMatch(self, expected_path, got_path):
    """Check two manifest files for equality.

    Args:
        expected_path: The path to the content we expect.
        got_path: The path to the content we got.
    """
    e_file = ContentManifestTest.run_files.Rlocation('rules_pkg/' + expected_path)
    with open(e_file, mode='rt', encoding='utf-8') as e_fp:
      expected = json.loads(e_fp.read())
    expected_dict = {x['dest']: x for x in expected}
    g_file = ContentManifestTest.run_files.Rlocation('rules_pkg/' + got_path)
    with open(g_file, mode='rt', encoding='utf-8') as g_fp:
      got = json.loads(g_fp.read())
    got_dict = {x['dest']: x for x in got}

    ok = True
    expected_dests = set(expected_dict.keys())
    got_dests = set(got_dict.keys())
    for dest, what in expected_dict.items():
      got = got_dict.get(dest)
      if got:
        # bzlmod mode changes root to @@//, but older version give @//
        origin = got.get('origin')
        if origin and origin.startswith('@@//'):
          got['origin'] = origin[1:]
        self.assertDictEqual(what, got)
      else:
        print('Missing expected path "%s" in manifest' % dest)
        ok = False
    for dest, what in got_dict.items():
      expected = expected_dict.get(dest)
      if expected:
        self.assertDictEqual(expected, what)
      else:
        print('Got unexpected path "%s" in manifest:' % dest, what)
        ok = False

    if not ok:
      print('To update the golden file:')
      print('  cp bazel-bin/%s %s' % (got_path, expected_path))
      print('or')
      print('============= snip ==========')
      print(got_dict.values())
      print('============= snip ==========')
    self.assertTrue(ok)
