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

"""Proof of a WORKSPACE SBOM generator.

This is only a demonstration. It will be replaced with other tools.
"""

import argparse
import codecs
import json
from tools import sbom
import subprocess
import os

TOOL = 'https//github.com/bazelbuild/rules_license/tools:write_workspace_sbom'

def main():
    parser = argparse.ArgumentParser(
      description='Demonstraton license compliance checker')

    parser.add_argument('--out', default='sbom.out', help='SBOM output')
    args = parser.parse_args()

    if "BUILD_WORKING_DIRECTORY" in os.environ:
        os.chdir(os.environ["BUILD_WORKING_DIRECTORY"])

    external_query_process = subprocess.run(
        ['bazel', 'query', '--output', 'streamed_jsonproto', '//external:*'],
        stdout=subprocess.PIPE,
    )
    sbom_packages = []
    for dep_string in external_query_process.stdout.decode('utf-8').splitlines():
        dep = json.loads(dep_string)
        if dep["type"] != "RULE":
            continue

        rule = dep["rule"]
        if rule["ruleClass"] == "http_archive":
            sbom_package = {}
            sbom_packages.append(sbom_package)
            
            if "attribute" not in rule:
                continue

            attributes = {attribute["name"]: attribute for attribute in rule["attribute"]}
            
            if "name" in attributes:
                sbom_package["package_name"] = attributes["name"]["stringValue"]
            
            if "url" in attributes:
                sbom_package["package_url"] = attributes["url"]["stringValue"]
            elif "urls" in attributes:
                urls = attributes["urls"]["stringListValue"]
                if urls and len(urls) > 0:
                    sbom_package["package_url"] = attributes["urls"]["stringListValue"][0]

    with codecs.open(args.out, mode='w', encoding='utf-8') as out:
        sbom_writer = sbom.SBOMWriter(TOOL, out)
        sbom_writer.write_header(package="Bazel's Workspace SBOM")
        sbom_writer.write_packages(packages=sbom_packages)

if __name__ == '__main__':
  main()
