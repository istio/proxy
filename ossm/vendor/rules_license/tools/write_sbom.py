#!/usr/bin/env python3
# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Proof of concept license checker.

This is only a demonstration. It will be replaced with other tools.
"""

import argparse
import codecs
import json
from tools import sbom

TOOL = 'https//github.com/bazelbuild/rules_license/tools:write_sbom'

def _load_package_data(package_info):
  with codecs.open(package_info, encoding='utf-8') as inp:
    return json.loads(inp.read())

def main():
  parser = argparse.ArgumentParser(
      description='Demonstraton license compliance checker')

  parser.add_argument('--licenses_info',
                      help='path to JSON file containing all license data')
  parser.add_argument('--out', default='sbom.out', help='SBOM output')
  args = parser.parse_args()

  license_data = _load_package_data(args.licenses_info)
  target = license_data[0]  # we assume only one target for the demo

  top_level_target = target['top_level_target']
  dependencies = target['dependencies']
  # It's not really packages, but this is close proxy for now
  licenses = target['licenses']
  package_infos = target['packages']

  # These are similar dicts, so merge them by package. This is not
  # strictly true, as different licenese can appear in the same
  # package, but it is good enough for demonstrating the sbom.

  all = {x['bazel_package']: x for x in licenses}
  for pi in package_infos:
    p = all.get(pi['bazel_package'])
    if p:
      p.update(pi)
    else:
      all[pi['bazel_package']] = pi

  with codecs.open(args.out, mode='w', encoding='utf-8') as out:
    sbom_writer = sbom.SBOMWriter(TOOL, out)
    sbom_writer.write_header(package=top_level_target)
    sbom_writer.write_packages(packages=all.values())


if __name__ == '__main__':
  main()
