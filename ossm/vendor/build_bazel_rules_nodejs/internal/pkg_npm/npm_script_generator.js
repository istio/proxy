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
 * @fileoverview This script generates npm pack & publish shell scripts from
 * a template.
 */
'use strict';

const fs = require('fs');

function main(args) {
  const
      [outDir, packPath, publishPath, runNpmTemplatePath, packBatPath, publishBatPath] = args;
  const cwd = process.cwd();
  if (/[\//]sandbox[\//]/.test(cwd)) {
    console.error('Error: npm_script_generator must be run with no sandbox');
    process.exit(1);
  }

  const npmTemplate = fs.readFileSync(runNpmTemplatePath, {encoding: 'utf-8'});
  fs.writeFileSync(packPath, npmTemplate.replace('TMPL_args', `pack "${cwd}/${outDir}"`));
  fs.writeFileSync(publishPath, npmTemplate.replace('TMPL_args', `publish "${cwd}/${outDir}"`));

  fs.writeFileSync(packBatPath, npmTemplate.replace('TMPL_args', `pack "${cwd}/${outDir}"`));
  fs.writeFileSync(publishBatPath, npmTemplate.replace('TMPL_args', `publish "${cwd}/${outDir}"`));
}

if (require.main === module) {
  process.exitCode = main(process.argv.slice(2));
}
