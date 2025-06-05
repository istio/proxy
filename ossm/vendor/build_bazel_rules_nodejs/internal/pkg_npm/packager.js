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
/**
 * @fileoverview This script copies files and nested packages into the output
 * directory of the pkg_npm rule.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const isBinary = require('isbinaryfile').isBinaryFileSync;

/**
 * Type definition describing a file captured in the Bazel action.
 * https://docs.bazel.build/versions/main/skylark/lib/File.html.
 * @typedef {{path: string, shortPath: string}} BazelFileInfo
 */

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

function copyWithReplace(src, dest, substitutions) {
  mkdirp(path.dirname(dest));
  if (fs.lstatSync(src).isDirectory()) {
    const files = fs.readdirSync(src)
    files.forEach((relativeChildSrc) => {
      const childSrc = path.join(src, relativeChildSrc);
      const childDest = path.join(dest, path.basename(childSrc));
      copyWithReplace(childSrc, childDest, substitutions);
    });
  } else if (!isBinary(src)) {
    let content = fs.readFileSync(src, {encoding: 'utf-8'});
    substitutions.forEach(r => {
      const [regexp, newvalue] = r;
      content = content.replace(regexp, newvalue);
    });
    fs.writeFileSync(dest, content);
  } else {
    fs.copyFileSync(src, dest);
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

function main(args) {
  args = fs.readFileSync(args[0], {encoding: 'utf-8'}).split('\n').map(unquoteArgs);
  const
      [outDir, owningPackageName, srcsArg, depsArg, packagesArg, substitutionsArg,
       volatileFile, infoFile, vendorExternalArg, target, validate, packageNameArg] = args;

  /** @type BazelFileInfo[] */
  const srcs = JSON.parse(srcsArg);

  /** @type BazelFileInfo[] */
  const deps = JSON.parse(depsArg);

  /** @type BazelFileInfo[] */
  const packages = JSON.parse(packagesArg);

  const vendorExternal = vendorExternalArg.split(',').filter(s => !!s);

  const substitutions = [
    // Strip content between BEGIN-INTERNAL / END-INTERNAL comments
    [/(#|\/\/)\s+BEGIN-INTERNAL[\w\W]+?END-INTERNAL/g, ''],
  ];
  const rawReplacements = JSON.parse(substitutionsArg);
  for (const key of Object.keys(rawReplacements)) {
    substitutions.push([new RegExp(key, 'g'), rawReplacements[key]])
  }
  // Replace statuses last so that earlier substitutions can add
  // status-related placeholders
  if (volatileFile || infoFile) {
    const statuses = {
      ...parseStatusFile(volatileFile),
      ...parseStatusFile(infoFile),
    };
    // Looks like {'BUILD_SCM_VERSION': 'v1.2.3'}
    for (let idx = 0; idx < substitutions.length; idx++) {
      const match = substitutions[idx][1].match(/\{(.*)\}/);
      if (!match) continue;
      const statusKey = match[1];
      let statusValue = statuses[statusKey];
      if (statusValue) {
        // npm versions must be numeric, so if the VCS tag starts with leading 'v', strip it
        // See https://github.com/bazelbuild/rules_nodejs/pull/1591
        if (statusKey.endsWith('_VERSION')) {
          statusValue = statusValue.replace(/^v/, '');
        }
        substitutions[idx][1] = statusValue;
      }
    }
  }

  // src like owningPackageName/my/path is just copied to outDir/my/path
  for (let srcFile of srcs) {
    if (srcFile.shortPath.startsWith('../')) {
      // If src is from external workspace drop the ../wksp portion
      copyWithReplace(srcFile.path, path.join(outDir,
          srcFile.shortPath.split('/').slice(2).join('/')), substitutions);
    } else {
      // Source is from local workspace
      if (!srcFile.path.startsWith(owningPackageName)) {
        throw new Error(
            `${srcFile.shortPath} in 'srcs' does not reside in the base directory, ` +
            `generated file should belong in 'deps' instead.`);
      }

      const outRelativePath = getOwningPackageRelativeOutPath(srcFile);

      if (validate === "true" && outRelativePath === "package.json") {
        const packageJson = JSON.parse(fs.readFileSync(srcFile.path, 'utf8'));
        if (packageJson['name'] !== packageNameArg) {
          console.error(`ERROR: pkg_npm rule ${
            target} was configured with attributes that don't match the package.json`);
          console.error(` - attribute package_name=${packageNameArg} does not match package.json name=${packageJson['name']}`)
          console.error('You can automatically fix this by running:');
          console.error(
              `    npx @bazel/buildozer 'set package_name "${packageJson['name']}"' ${target}`);
          console.error('Or to suppress this error, run:');
          console.error(`    npx @bazel/buildozer 'set validate False' ${target}`);
          return 1;
        }
      }
      copyWithReplace(srcFile.path, path.join(outDir, outRelativePath), substitutions);
    }
  }

  /**
   * Gets the output path for the given file, relative to the output package directory.
   * e.g. if the file path is `bazel-out/<..>/bin/packages/core/index.js` and the
   * owning package is `packages/core`, then `index.js` is being returned.
   * @param file {BazelFileInfo}
   * @returns {string}
   */
  function getOwningPackageRelativeOutPath(file) {
    for (const workspaceName of vendorExternal) {
      if (file.shortPath.startsWith(`../${workspaceName}`)) {
        return path.relative(`../${workspaceName}`, file.shortPath);
      }
    }

    return path.relative(owningPackageName, file.shortPath);
  }

  // Deps like bazel-bin/baseDir/my/path is copied to outDir/my/path.
  for (const dep of deps) {
    const outPath = path.join(outDir, getOwningPackageRelativeOutPath(dep));
    try {
      copyWithReplace(dep.path, outPath, substitutions);
    } catch (e) {
      console.error(`Failed to copy ${dep} to ${outPath}`);
      throw e;
    }
  }

  // package contents like bazel-bin/baseDir/my/directory/* is
  // recursively copied to outDir/my/*
  for (const pkg of packages) {
    const outRelativePath = path.dirname(getOwningPackageRelativeOutPath(pkg));
    const outExecPath = path.join(outDir, outRelativePath);

    function copyRecursive(base, file) {
      const absolutePath = path.join(base, file);
      file = file.replace(/\\/g, '/');

      if (fs.lstatSync(absolutePath).isDirectory()) {
        const files = fs.readdirSync(absolutePath);

        files.forEach(f => {
          copyRecursive(base, path.join(file, f));
        });
      } else {
        copyWithReplace(absolutePath, path.join(outExecPath, file), substitutions);
      }
    }
    fs.readdirSync(pkg.path).forEach(f => {
      copyRecursive(pkg.path, f);
    });
  }
}

if (require.main === module) {
  process.exitCode = main(process.argv.slice(2));
}
