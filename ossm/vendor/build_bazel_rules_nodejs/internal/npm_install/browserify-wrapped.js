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

// A wrapper around browserify necessary since an absolute path is needed
// for browserify to find the named-amd plugin

const child_process = require('child_process');

function log_verbose(...m) {
  if (!!process.env['VERBOSE_LOGS']) console.error('[browserify-wrapped.js]', ...m);
}

const BABEL_PLUGINS = [
  '@babel/plugin-transform-modules-commonjs',
];

runBrowserify(...process.argv.slice(2));

function runBrowserify(workspaceName, packageName, entryPoint, output, excluded = '') {
  log_verbose(`running with
  cwd: ${process.cwd()}
  workspaceName: ${workspaceName},
  packageName: ${packageName}
  entryPoint: ${entryPoint}
  output: ${output}
  excluded: ${excluded}`);

  const browserify = require.resolve(`browserify/index.js`);
  const namedAmd = require.resolve('named-amd');
  const babelify = require.resolve('babelify');
  const plugins = BABEL_PLUGINS.map(p => require.resolve(p));

  let args = [
    browserify, entryPoint, '--preserve-symlinks', '-t', '[', babelify, '--plugins', '[',
    ...plugins, ']', ']',
    // Supply the name to use for the AMD define with named-amd plugin
    '-p', '[', namedAmd, '--name', packageName, ']',
    // Output a stand-alone UMD bundle. Sanitized version the supplied name is used for
    // the global name (see https://github.com/ForbesLindesay/umd#name-casing-and-characters).
    // Global name set to `browserify_${workspaceName}_${packageName}` so there are no
    // conflicts with other globals.
    '-s', `browserify_${workspaceName}_${packageName}`, '-o', output
  ];
  for (const e of excluded.split(',')) {
    args = args.concat(['-u', e])
  }

  log_verbose(`running node ${args.join(' ')}\n`);

  const isWindows = /^win/i.test(process.platform);
  child_process.execFileSync(
      isWindows ? 'node.exe' : 'node', args,
      {stdio: [process.stdin, process.stdout, process.stderr]});
}
