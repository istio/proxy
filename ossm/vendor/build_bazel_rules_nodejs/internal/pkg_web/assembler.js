/**
 * @license
 * Copyright 2018 The Bazel Authors. All rights reserved.
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

const fs = require('fs');
const path = require('path');
const isBinary = require('isbinaryfile').isBinaryFileSync;

/**
 * Create a new directory and any necessary subdirectories
 * if they do not exist.
 */
function mkdirp(p) {
  if (!fs.existsSync(p)) {
    mkdirp(path.dirname(p));
    fs.mkdirSync(p);
  }
}

function unquoteArgs(s) {
  return s.replace(/^'(.*)'$/, '$1');
}

/**
* The status files are expected to look like
* BUILD_SCM_HASH 83c699db39cfd74526cdf9bebb75aa6f122908bb
* BUILD_SCM_LOCAL_CHANGES true
* STABLE_BUILD_SCM_VERSION 6.0.0-beta.6+12.sha-83c699d.with-local-changes
* BUILD_TIMESTAMP 1520021990506
*
* Parsing regex is created based on Bazel's documentation describing the status file schema:
*   The key names can be anything but they may only use upper case letters and underscores. The
*   first space after the key name separates it from the value. The value is the rest of the line
*   (including additional whitespaces).
*
* @param {string} p the path to the status file
* @returns a two-dimensional array of key/value pairs
*/
function parseStatusFile(p) {
  if (!p) return [];
  const results = {};
  const statusFile = fs.readFileSync(p, {encoding: 'utf-8'});
  for (const match of `\n${statusFile}`.matchAll(/^([A-Z_]+) (.*)/gm)) {
    // Lines which go unmatched define an index value of `0` and should be skipped.
    if (match.index === 0) {
      continue;
    }
    results[match[1]] = match[2];
  }
  return results;
}

function normalizeSubstitutions(substitutionsArg, stampMap) {
  const substitutions = JSON.parse(substitutionsArg);

  const normalizedSubstitutions = {};

  for (const occurrence in substitutions) {
    let substituteWith = substitutions[occurrence];
    if (substituteWith.match(/^{.*?}$/)) {
      substituteWith = substituteWith.replace(/^{(.*?)}$/, '$1');
      if (!stampMap[substituteWith]) {
        throw new Error(`Could not find ${substituteWith} key in status file.`);
      }
      substituteWith = stampMap[substituteWith];
    }
    normalizedSubstitutions[occurrence] = substituteWith;
  }
  return normalizedSubstitutions;
}

function main(params) {
  const outdir = params.shift();

  const volatileFilePath = params.shift();

  const stableFilePath = params.shift();

  const rawSubstitutions = params.shift().replace(/^'(.*)'$/, '$1');

  const stampMap = {
    ...parseStatusFile(volatileFilePath),
    ...parseStatusFile(stableFilePath),
  };

  const normalizedSubstitutions = normalizeSubstitutions(rawSubstitutions, stampMap)

  const substitutions = Object.entries(normalizedSubstitutions);

  const rootDirs = [];
  while (params.length && params[0] !== '--assets') {
    let r = params.shift();
    if (!r.endsWith('/')) {
      r += '/';
    }
    rootDirs.push(r);
  }
  // Always trim the longest prefix
  rootDirs.sort((a, b) => b.length - a.length);

  params.shift(); // --assets

  function relative(execPath) {
    if (execPath.startsWith('external/')) {
      execPath = execPath.substring('external/'.length);
    }
    for (const r of rootDirs) {
      if (execPath.startsWith(r)) {
        return execPath.substring(r.length);
      }
    }
    return execPath;
  }

  function copy(f, substitutions) {
    if (fs.statSync(f).isDirectory()) {
      for (const file of fs.readdirSync(f)) {
        // Change paths to posix
        copy(path.join(f, file).replace(/\\/g, '/'), substitutions);
      }
    } else if (!isBinary(f)) {
      const dest = path.join(outdir, relative(f));
      let content = fs.readFileSync(f, {encoding: 'utf-8'});
      substitutions.forEach(([occurrence, replaceWith]) => {
        content = content.replace(occurrence, replaceWith);
      });
      fs.mkdirSync(path.dirname(dest), {recursive: true});
      fs.writeFileSync(dest, content);
    } else {
      const dest = path.join(outdir, relative(f));
      mkdirp(path.dirname(dest));
      fs.copyFileSync(f, dest);
    }
  }

  // Remove duplicate files (which may come from this rule) from the
  // list since fs.copyFileSync may fail with `EACCES: permission denied`
  // as it will not have permission to overwrite duplicate files that were
  // copied from within bazel-bin.
  // See https://github.com/bazelbuild/rules_nodejs/pull/546.
  for (const f of new Set(params)) {
    copy(f, substitutions);
  }
  return 0;
}

module.exports = {main};

if (require.main === module) {
  // We always require the arguments are encoded into a flagfile
  // so that we don't exhaust the command-line limit.
  const params = fs.readFileSync(process.argv[2], {encoding: 'utf-8'})
                     .split('\n')
                     .filter(l => !!l)
                     .map(unquoteArgs);
  process.exitCode = main(params);
}
