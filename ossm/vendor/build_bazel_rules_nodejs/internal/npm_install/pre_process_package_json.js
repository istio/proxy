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
/**
 * @fileoverview This script reads the package.json file
 * of a yarn_install or npm_install rule and performs steps
 * that may be required before running yarn or npm such as
 * clearing the yarn cache for `file://` URIs to work-around
 * https://github.com/yarnpkg/yarn/issues/2165.
 */
'use strict';

const fs = require('fs');
const child_process = require('child_process');

function log_verbose(...m) {
  if (!!process.env['VERBOSE_LOGS']) console.error('[pre_process_package_json.js]', ...m);
}

const args = process.argv.slice(2);
const packageJson = args[0];
const packageManager = args[1];

if (require.main === module) {
  main();
}

/**
 * Main entrypoint.
 */
function main() {
  const isYarn = (packageManager === 'yarn');

  const pkg = JSON.parse(fs.readFileSync(packageJson, {encoding: 'utf8'}));

  log_verbose(`pre-processing package.json`);

  if (isYarn) {
    // Work-around for https://github.com/yarnpkg/yarn/issues/2165
    // Note: there is no equivalent npm functionality to clean out individual packages
    // from the npm cache.
    clearYarnFilePathCaches(pkg);
  }
}

/**
 * Runs `yarn cache clean` for all packages that have `file://` URIs.
 * Work-around for https://github.com/yarnpkg/yarn/issues/2165.
 */
function clearYarnFilePathCaches(pkg) {
  const fileRegex = /^file\:\/\//i;
  const clearPackages = [];

  if (pkg.dependencies) {
    Object.keys(pkg.dependencies).forEach(p => {
      if (pkg.dependencies[p].match(fileRegex)) {
        clearPackages.push(p);
      }
    });
  }
  if (pkg.devDependencies) {
    Object.keys(pkg.devDependencies).forEach(p => {
      if (pkg.devDependencies[p].match(fileRegex)) {
        clearPackages.push(p);
      }
    });
  }
  if (pkg.optionalDependencies) {
    Object.keys(pkg.optionalDependencies).forEach(p => {
      if (pkg.optionalDependencies[p].match(fileRegex)) {
        clearPackages.push(p);
      }
    });
  }

  if (clearPackages.length) {
    log_verbose(`cleaning packages from yarn cache: ${clearPackages.join(' ')}`);
    for (const c of clearPackages) {
      child_process.execFileSync(
          'yarn', ['--mutex', 'network', 'cache', 'clean', c],
          {stdio: [process.stdin, process.stdout, process.stderr]});
    }
  }
}

module.exports = {main};
