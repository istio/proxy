/**
 * @license
 * Copyright 2017 The Bazel Authors. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
'use strict';

const runfiles = require(process.env['BAZEL_NODE_RUNFILES_HELPER']);
const fs = require('fs');
const args = process.argv.slice(2);

const BAZEL_VERSION = args[0];

const version =
    fs.readFileSync(runfiles.resolve('build_bazel_rules_nodejs/.bazelversion'), 'utf-8').trim();
const bazelci_version =
    fs.readFileSync(runfiles.resolve('build_bazel_rules_nodejs/.bazelci/presubmit.yml'), 'utf-8')
        .split('\n')
        .find(v => v.startsWith('bazel:'));

// Test that the BAZEL_VERSION defined in //:index.bzl is in sync with .bazelversion and .bazelci
if (version !== BAZEL_VERSION || bazelci_version.split(':')[1].trim() !== BAZEL_VERSION) {
  console.error(`Bazel versions do not match.
      //:index.bzl BAZEL_VERSION='${BAZEL_VERSION}'
      //:.bazelversion '${version}'
      //:.bazelci/presubmit.yml '${bazelci_version}'`);
  process.exitCode = 1;
}
